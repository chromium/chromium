# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
from typing import Optional

import logging
import packaging
import pytest

from chrome.test.variations import test_utils
from chrome.test.variations.drivers import DriverFactory
from chrome.test.variations.fixtures.test_options import TestOptions
from chrome.test.variations.fixtures.result_sink import AddArtifact


_PLATFORM_TO_RELEASE_OS = {
  'linux': 'linux',
  'mac': 'mac_arm64',
  'win': 'win64',
  'android': 'android',
  'webview': 'webview',
  'android_webview': 'webview',
}

def pytest_addoption(parser):
  parser.addoption('--chromedriver',
                   help='The path to the existing chromedriver. '
                   'This will ignore --channel and skip downloading.')

  # Options for android emulators
  parser.addoption(
    '--avd-config',
    type=os.path.realpath,
    help=('Path to the avd config. Required for Android products. '
          '(See //tools/android/avd/proto for message definition '
          'and existing *.textpb files.)'))

  parser.addoption(
    '--emulator-window',
    action='store_true',
    default=False,
    help='Enable graphical window display on the emulator.')

  # Options for CrOS VMs.
  parser.addoption('--board',
                   default='betty-pi-arc',
                   help='The board name of the CrOS VM.')


def _version_to_download(
  chrome_version, platform, channel) -> packaging.version.Version:
  # Use the explicitly passed in version, if any.
  if chrome_version:
    logging.info(
      'using --chrome-version to download chrome (ignoring --channel)')
    return test_utils.parse_version(chrome_version)

  release_os = _PLATFORM_TO_RELEASE_OS.get(platform, None)
  if release_os is None:
    logging.info('No Chrome version to download for ' + platform)
    return None
  else:
    return test_utils.find_version(release_os, channel)

# pylint: disable=redefined-outer-name
@pytest.fixture(scope="session")
def chromedriver_path(pytestconfig, test_options: TestOptions) -> Optional[str]:
  """Finds the path to the chromedriver.

  The fixture downloads the chromedriver from GCS bucket from a given
  channel or version for the host platform. It will reuse the existing
  one if the path is given from the command line.

  This fixture also attempts to download Chrome binaries if needed for
  certain platforms so the chromedriver can launch Chrome as well.

  Note. Chrome and chromedriver can be two separate fixtures but they
  are combines for now.

  Returns:
    The path to the chromedriver if the platform is supported.
    None if the platform is not supported.
  """
  if cd_path := pytestconfig.getoption('chromedriver'):
    cd_path = os.path.abspath(cd_path)
    assert os.path.isfile(cd_path), (
      f'Given chromedriver doesn\'t exist. ({cd_path})')
    return cd_path

  platform = test_options.platform
  channel = test_options.channel
  chrome_version = test_options.chrome_version

  version = str(_version_to_download(chrome_version, platform, channel))

  # https://developer.chrome.com/docs/versionhistory/reference/#platform-identifiers
  chrome_dir = None
  if platform == "linux":
    chrome_dir = test_utils.download_chrome_linux(version=version)
  elif platform == "mac":
    chrome_dir = test_utils.download_chrome_mac(version=version)
  elif platform == "win":
    chrome_dir = test_utils.download_chrome_win(version=version)
  elif platform in ('android', 'webview', 'android_webview'):
    # For Android/Webview, we will use install_webview or install_chrome to
    # download and install APKs, however, we will still need the chromedriver
    # binaries for the hosts. Currently we will only run on Linux, so fetching
    # the chromedriver for Linux only.
    pass
  elif platform in ('lacros', 'cros'):
    # CrOS and LaCrOS running inside a VM, the chromedriver will run on the
    # Linux host. The browser is started inside VM through dbus message, and
    # the debugging port is forwarded. see drivers/chromeos.py.
    pass
  else:
    # the platform is not supported.
    return None

  hosted_platform = test_utils.get_hosted_platform()
  chromedriver_path = test_utils.download_chromedriver(
    hosted_platform,
    # We use the latest chromedriver whenever possible. The drivers for Windows
    # are not backward compatible so we use the same version as Chrome.
    version if hosted_platform == 'win' else None,
  )

  # If we also download Chrome binary, move the chromedriver to the Chrome
  # folder so the chromedriver can find it in the same folder
  if chrome_dir:
    chromedriver_in_chrome = os.path.join(chrome_dir,
                                          os.path.basename(chromedriver_path))
    shutil.move(chromedriver_path, chromedriver_in_chrome)
    chromedriver_path = chromedriver_in_chrome

  return chromedriver_path


@pytest.fixture(autouse=True)
def driver_logs(driver_factory: DriverFactory, add_artifact: AddArtifact):
  # the driver factor increases the counter before the session is created.
  session_start = driver_factory.driver_session_counter + 1
  yield
  session_end = driver_factory.driver_session_counter + 1
  for session_count in range(session_start, session_end):
    session_folder = driver_factory.get_driver_session_folder(session_count)
    for filename in os.listdir(session_folder):
      add_artifact(f'Session-{session_count}-Files {filename}',
                   os.path.join(session_folder, filename))


@pytest.fixture(scope='session')
def driver_factory(
  pytestconfig,
  chromedriver_path: str,
  tmp_path_factory: pytest.TempPathFactory,
  local_http_server: 'HTTPServer',
  ) -> DriverFactory:
  """Returns a factory that creates a webdriver."""
  factory: Optional[DriverFactory] = None
  target_platform = pytestconfig.getoption('target_platform')
  artifacts_path = tmp_path_factory.mktemp('artifacts')
  if target_platform in ('linux', 'win', 'mac'):
    from chrome.test.variations.drivers import desktop
    factory = desktop.DesktopDriverFactory(
      channel=pytestconfig.getoption('channel'),
      crash_dump_dir=str(tmp_path_factory.mktemp('crash')),
      artifacts_path=str(artifacts_path),
      chromedriver_path=chromedriver_path)
  elif target_platform in ('android', 'webview', 'android_webview'):
    assert test_utils.get_hosted_platform() == 'linux', (
      f'Only support to run android tests on Linux, but running on '
      f'{test_utils.get_hosted_platform()}'
    )
    from chrome.test.variations.drivers import android
    factories = {
      'android': android.AndroidDriverFactory,
      'webview': android.WebviewDriverFactory,
      'android_webview': android.WebviewDriverFactory,
    }

    factory = factories[target_platform](
      channel=pytestconfig.getoption('channel'),
      avd_config=pytestconfig.getoption('avd_config'),
      enabled_emulator_window=pytestconfig.getoption('emulator_window'),
      artifacts_path=str(artifacts_path),
      chromedriver_path=chromedriver_path,
      ports=[local_http_server.server_port]
    )
  elif target_platform in ('cros'):
    from chrome.test.variations.drivers import chromeos
    factory = chromeos.CrOSDriverFactory(
      channel=pytestconfig.getoption('channel'),
      board=pytestconfig.getoption('board'),
      artifacts_path=str(artifacts_path),
      chromedriver_path=chromedriver_path,
      server_port=local_http_server.server_port
      )

  if not factory:
    assert False, f'Not supported platform {target_platform}.'

  try:
    yield factory
  finally:
    factory.close()
