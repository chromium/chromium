# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Android module to prepare APKs and emulators to run webdriver-based tests."""

import re
import os
import subprocess
import sys

import packaging
import logging
from typing import List, Optional

from chrome.test.variations.test_utils import SRC_DIR
from chrome.test.variations.test_utils.helper import find_gsutil_cmd

# The root for the module pylib/android is under build/android.
sys.path.append(os.path.join(SRC_DIR, 'build', 'android'))

# This import adds `devil` to `sys.path`.
import devil_chromium  # noqa: F401

from devil.android import apk_helper  # noqa: F401
from devil.android import device_utils
from devil.android import forwarder
from devil.android.sdk import adb_wrapper
from pylib.local.emulator import avd

_INSTALLER_SCRIPT_PY = os.path.join(
  SRC_DIR, 'clank', 'bin', 'utils', 'installer_script_wrapper.py'
)


def _package_name(channel: str):
  if channel in ('beta', 'dev', 'canary'):
    return f'com.chrome.{channel}'
  return 'com.android.chrome'


def _is_require_signed(channel: str) -> bool:
  """Check if we need to install a signed build."""
  # The stable build has the same package name as prebuilt one, in order
  # to avoid the signature mismatch, we need to install the one with the
  # same signed build.
  return channel == 'stable'


def _installer_args(
  product: str, channel: str, device: device_utils.DeviceUtils
) -> List[str]:
  """Returns the command to install `product` from the release bucket."""
  args = [
    _INSTALLER_SCRIPT_PY,
    f'--product={product}',
    f'--channel={channel}',
    f'--serial={device.serial}',
    f'--adb={adb_wrapper.AdbWrapper.GetAdbPath()}',
    f'--gsutil={find_gsutil_cmd()}',
  ]
  args.append('--signed' if _is_require_signed(channel) else '--unsigned')
  return args


def install_chrome(
  channel: str, device: device_utils.DeviceUtils, package: str
) -> str:
  """Installs Chrome to the device and returns the package name.

  Args:
    channel: The release channel to install.
    device: The device to install Chrome on.
    package: The installer package variant, e.g. 'Chrome' for phones and
      tablets or 'ChromeDesktop' for Android desktop devices.

  Returns:
    The Android package name of the installed Chrome, e.g. 'com.chrome.dev'.

  Raises:
    RuntimeError: If the installer fails.
  """
  args = _installer_args('chrome', channel, device)
  # The package is always passed explicitly. For phones, 'Chrome' can be
  # dropped once clank/bin/install_chrome.py defaults to Standalone Chrome for
  # M154+ (b/563029158). 'ChromeDesktop' must stay, otherwise the installer
  # may install the phone APK on Android desktop devices.
  args.append(f'--package={package}')
  try:
    output = subprocess.check_output(args=args, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error(
      'Subprocess error caught %s', e.output.decode(errors='replace')
    )
    raise RuntimeError('Chrome installation failed.')
  logging.info('Chrome installer output:\n%s', output.decode(errors='replace'))
  return _package_name(channel)


def install_webview(
  channel: str, device: device_utils.DeviceUtils
) -> packaging.version.Version:
  """Installs Webview to the device and returns the installed version."""
  args = _installer_args('webview', channel, device)
  try:
    subprocess.check_output(args=args, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Subprocess error caught %s', e.output.decode())
    raise RuntimeError('Webview installation failed.')

  version_regex = r'\s*Preferred WebView package[^:]*[^\d]*([^\)]+)'
  version_output = device.RunShellCommand(['dumpsys', 'webviewupdate'])
  version = [
    m.group(1)
    for line in version_output
    if (m := re.match(version_regex, line))
  ]
  return packaging.version.parse(version[0]) if version else None


def _forward_port(
  device: device_utils.DeviceUtils, ports: Optional[List[int]] = None
):
  # Ideally, we would dynamically allocate ports from the device, and
  # remember the mapping here, it requires how the client redirects ports.
  # Since we currently only allocate ports from a user space whose value is
  # always 3xxxx and above, there is a very rare case to cause issues here.
  # It is possible that the port is already used on the device, however,
  # the likelihood is small, and we will fix once it shows a problem.
  if ports:
    forwarder.Forwarder.Map([(port, port) for port in ports], device)


def launch_emulator(
  avd_config: str,
  emulator_window: bool,
  ports: Optional[List[int]] = None,
  ensure_main_user: bool = False,
) -> avd._AvdInstance:
  """Launches the emulator and forwards ports from device to host.

  Args:
    avd_config: Path to the AVD config textpb.
    emulator_window: Whether to show the emulator window.
    ports: Host ports to forward to the device.
    ensure_main_user: Whether to switch to and unlock the main user after
      boot. Needed on images with a headless system user (e.g. Android
      desktop), where apps otherwise never get a visible window.
  """
  avd_config = avd.AvdConfig(avd_config)
  avd_config.Install()

  instance = avd_config.CreateInstance()
  instance.Start(writable_system=True, window=emulator_window)

  if ensure_main_user:
    user_id = instance.device.EnsureMainUser()
    logging.info('Switched to the main user (%s)', user_id)

  _forward_port(instance.device, ports)

  return instance
