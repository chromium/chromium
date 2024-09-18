# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Android module to prepare APKs and emulators to run webdriver-based tests.
"""

import re
import os
import subprocess
import sys

import packaging
from typing import List, Optional

from chrome.test.variations.test_utils import SRC_DIR

# The root for the module pylib/android is under build/android.
sys.path.append(os.path.join(SRC_DIR, 'build', 'android'))

# This import adds `devil` to `sys.path`.
import devil_chromium

from devil.android import apk_helper
from devil.android import device_utils
from devil.android import forwarder
from devil.android.sdk import adb_wrapper
from pylib.local.emulator import avd

_INSTALLER_SCRIPT_PY = os.path.join(
  SRC_DIR, 'clank', 'bin', 'utils', 'installer_script_wrapper.py')


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


def install_chrome(channel: str, device: device_utils.DeviceUtils) -> str:
  """Installs Chrome to the device and returns the package name."""
  args = [
    _INSTALLER_SCRIPT_PY, f'--product=chrome',
    f'--channel={channel}', f'--serial={device.serial}',
    f'--adb={adb_wrapper.AdbWrapper.GetAdbPath()}',
  ]
  args.append('--signed' if _is_require_signed(channel) else '--unsigned')
  subprocess.check_call(args=args)
  return _package_name(channel)


def install_webview(
  channel: str,
  device: device_utils.DeviceUtils
  ) -> packaging.version.Version:
  """Installs Webview to the device and returns the installed version."""
  args = [
    _INSTALLER_SCRIPT_PY, f'--product=webview',
    f'--channel={channel}', f'--serial={device.serial}',
    f'--adb={adb_wrapper.AdbWrapper.GetAdbPath()}',
  ]
  args.append('--signed' if _is_require_signed(channel) else '--unsigned')
  subprocess.check_call(args=args)

  version_regex = r'\s*Preferred WebView package[^:]*[^\d]*([^\)]+)'
  version_output = device.RunShellCommand(['dumpsys' ,'webviewupdate'])
  version = [
    m.group(1)
    for line in version_output if (m := re.match(version_regex, line))
  ]
  return packaging.version.parse(version[0]) if version else None


def _forward_port(device: device_utils.DeviceUtils,
                  ports: Optional[List[int]] = None):
  # Ideally, we would dynamically allocate ports from the device, and
  # remember the mapping here, it requires how the client redirects ports.
  # Since we currently only allocate ports from a user space whose value is
  # always 3xxxx and above, there is a very rare case to cause issues here.
  # It is possible that the port is already used on the device, however,
  # the likelihood is small, and we will fix once it shows a problem.
  if ports:
    forwarder.Forwarder.Map([(port, port) for port in ports], device)


def launch_emulator(avd_config: str,
                    emulator_window: bool,
                    ports: Optional[List[int]] = None) -> avd._AvdInstance:
  """Launches the emulator and forwards ports from device to host."""
  avd_config = avd.AvdConfig(avd_config)
  avd_config.Install()

  instance = avd_config.CreateInstance()
  instance.Start(writable_system=True,
                 window=emulator_window,
                 require_fast_start=True)

  _forward_port(instance.device, ports)

  return instance
