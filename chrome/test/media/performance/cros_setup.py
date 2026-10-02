# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Starts Chrome on a ChromeOS sender through Crossbench."""

import importlib.abc
import importlib.util
import logging
import re
import sys
import time
from unittest.mock import MagicMock

import perf_config
import senders

# Crossbench only exposes the Selenium driver and window setup as private
# members, so this module has to reach into them.
# pylint: disable=protected-access

# Optional Crossbench dependencies that the perf bots do not install.
_MOCKED_PACKAGES = [
    "google.cloud", "psutil", "xlsxwriter", "hjson", "mobly",
    "snippet_uiautomator"
]
# Geometry flags are handled by the Viewport, and passing them to the
# autologin launch script crashes it.
_EXCLUDED_FLAGS = [
    '--window-size', '--window-position', '--start-maximized',
    '--start-fullscreen'
]
_CROS_CHROME_PATH = "/opt/google/chrome/chrome"


class _MockFinder(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    """Serves a MagicMock for any import under `mocked_packages`."""

    def __init__(self, mocked_packages):
        self.mocked_packages = mocked_packages

    def find_spec(self, fullname, path, target=None):
        del path, target
        if any(fullname == pkg or fullname.startswith(pkg + '.')
               for pkg in self.mocked_packages):
            return importlib.util.spec_from_loader(fullname, self)
        return None

    def create_module(self, spec):
        mock = MagicMock()
        # Ensure the mock is treated as a package for submodule imports.
        mock.__path__ = []
        return mock

    def exec_module(self, module):
        pass


def _prepare_crossbench_imports():
    """Makes Crossbench importable without its optional dependencies."""
    sys.meta_path.insert(0, _MockFinder(_MOCKED_PACKAGES))
    try:
        # pylint: disable-next=import-outside-toplevel
        import google.protobuf.runtime_version
        google.protobuf.runtime_version.ValidateProtobufRuntimeVersion = (
            lambda *args, **kwargs: None)
    except (ImportError, AttributeError):
        pass
    sys.path.insert(0, perf_config.CROSSBENCH_ROOT)


def _detect_milestone(cb_platform):
    """Returns the milestone (e.g. '129') of Chrome on the DUT, or None."""
    try:
        version_str = cb_platform.app_version(_CROS_CHROME_PATH)
        version_match = re.search(r'(\d+\.\d+\.\d+\.\d+)', version_str)
        if version_match:
            actual_version = version_match.group(1)
        else:
            # Fallback to the previous split logic if regex fails.
            actual_version = version_str.split()[-2]

        milestone = actual_version.split('.')[0]
        logging.info("Detected remote Chrome version: %s (milestone: %s)",
                     actual_version, milestone)
        return milestone
    except Exception as e:  # pylint: disable=broad-exception-caught
        logging.warning("Failed to detect remote Chrome version: %s", e)
        return None


def _install_chromedriver(sender, milestone):
    """Installs a chromedriver matching `milestone` on the DUT.

    Crossbench's SSH platform expects the driver to be on the remote device.

    Returns:
        The remote chromedriver path, or None if the install failed.
    """
    try:
        if not milestone:
            logging.warning("Milestone is None. download_cft_urls will fall "
                            "back to the latest version, which may cause a "
                            "mismatch.")
        _, _, driver_url = senders.download_cft_urls('linux64', milestone)
        sender.install_chrome(milestone)
        driver_dir = driver_url.split('/')[-1].replace('.zip', '')
        return f"{sender.TMP_DIR}/{driver_dir}/chromedriver"
    except Exception as e:  # pylint: disable=broad-exception-caught
        logging.warning("Failed to install matching ChromeDriver: %s", e)
        return None


def _crossbench_flags(chrome_options_list):
    """Converts Chrome switches to Crossbench's (name, value) flag format."""
    flags = []
    for flag in chrome_options_list:
        if any(f in flag for f in _EXCLUDED_FLAGS):
            continue
        if '=' in flag:
            flags.append(tuple(flag.split('=', 1)))
        else:
            flags.append(flag)

    # Force the window to launch in a maximized state.
    flags.append(("--window-state", "maximized"))
    flags.append(('--log-file', '/tmp/chrome_debug.log'))
    return flags


def setup_cros_environment(args, chrome_version, chrome_options_list):
    """Starts Chrome on the ChromeOS DUT and returns a Selenium driver.

    Returns:
        (Selenium driver, Crossbench platform, Chrome version string)
    """
    del chrome_version  # The DUT's own Chrome version is used instead.
    sender = senders.make_sender(args)
    sender.verify_connectivity()
    logging.info("Setting up ChromeOS environment using Crossbench.")

    _prepare_crossbench_imports()
    # pylint: disable=import-outside-toplevel
    from crossbench.plt.chromeos_ssh import ChromeOsSshPlatform
    from crossbench.plt import PLATFORM as host_platform
    from crossbench.browsers.chrome.webdriver import ChromeWebDriverChromeOsSsh
    from crossbench.browsers.settings import Settings
    from crossbench.browsers.viewport import Viewport
    # pylint: enable=import-outside-toplevel

    cb_platform = ChromeOsSshPlatform(host_platform,
                                      host=args.sender,
                                      port=0,
                                      ssh_port=22,
                                      ssh_user=args.username)

    # Enable detailed logging for Crossbench to debug autologin issues.
    logging.getLogger('crossbench').setLevel(logging.DEBUG)

    # Aggressively clear any leaked sessions.
    logging.info("Purging stale Chrome processes on device...")
    cb_platform.sh("pkill", "-9", "chrome", check=False)
    time.sleep(1)

    # Use the DUT's Chrome milestone to pick a matching chromedriver.
    milestone = _detect_milestone(cb_platform)
    remote_driver_path = _install_chromedriver(sender, milestone)

    chrome_os_flags = _crossbench_flags(chrome_options_list)
    settings = Settings(flags=chrome_os_flags,
                        platform=cb_platform,
                        driver_path=remote_driver_path,
                        viewport=Viewport.MAXIMIZED)

    # We must explicitly provide the binary path on ChromeOS.
    browser = ChromeWebDriverChromeOsSsh(label="cros_perf_test",
                                         path=_CROS_CHROME_PATH,
                                         settings=settings)

    # Crossbench filters out geometry flags by default for ChromeOS in
    # '_filter_flags_for_run'. We override this behavior to ensure our flags
    # reach the launch script (autologin.py).
    browser.UNSUPPORTED_FLAGS += ("--user-data-dir", )

    def _safe_setup_window():
        for _ in range(20):
            try:
                handles = browser._private_driver.window_handles
                if handles:
                    browser._private_driver.switch_to.window(handles[0])
                    return
            except Exception:  # pylint: disable=broad-exception-caught
                pass
            time.sleep(0.5)

    browser._setup_window = _safe_setup_window

    logging.info("Starting Crossbench Browser on ChromeOS...")
    # Crossbench's start() expects a session/run group.
    mock_session = MagicMock()
    mock_session.timing.timeout_unit = None
    mock_session.out_dir = perf_config.RECORDINGS_DIR
    # Prevent Crossbench from trying to use mock secrets for login.
    mock_session.browser.secrets.google = None

    server_port = perf_config.SERVER_PORT
    # Crossbench requires the network to be 'open' before starting the browser.
    with browser.network.open(mock_session):
        # Reverse-forward the local HTTP server so the DUT can reach it.
        logging.info("Setting up reverse port forwarding for port %d...",
                     server_port)
        try:
            cb_platform.ports.stop_reverse_forward(server_port)
        except Exception:  # pylint: disable=broad-exception-caught
            pass
        cb_platform.ports.reverse_forward(server_port, server_port)
        logging.info("Final ChromeOS flags: %s", chrome_os_flags)
        try:
            browser.start(mock_session)
        except Exception:
            logging.error("Browser failed to start.")
            raise

    driver = browser._private_driver
    actual_version = str(browser.version)
    logging.info("Detected remote Chrome version: %s", actual_version)
    return driver, cb_platform, actual_version
