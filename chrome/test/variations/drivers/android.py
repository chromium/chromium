# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
import posixpath

from contextlib import contextmanager
from typing import List, Optional

import attr

from chrome.test.variations.drivers import DriverFactory
# This import also adds `devil` and `build/android` to `sys.path`.
from chrome.test.variations.test_utils import android
from selenium import webdriver

from devil.android import device_temp_file
from devil.android import device_utils
from devil.android.sdk import intent


@attr.attrs()
class AndroidDriverFactory(DriverFactory):
  channel: str = attr.attrib()
  avd_config: Optional[str] = attr.attrib()
  enabled_emulator_window: bool = attr.attrib()
  ports: List[int] = attr.attrib()

  #override
  def __attrs_post_init__(self):
    super().__attrs_post_init__()
    self._instance = android.launch_emulator(
      avd_config=self.avd_config,
      emulator_window=self.enabled_emulator_window,
      ports=self.ports)
    self._device_temp_dir = device_temp_file.NamedDeviceTemporaryDirectory(
      self.device.adb)
    self._install_package()

  def _install_package(self):
    self._package_name = android.install_chrome(self.channel, self.device)
    self.device.ClearApplicationState(self.package_name)
    logging.info('Installed Chrome (%s)', self.package_name)

  #override
  @property
  def supports_startup_timeout(self) -> bool:
    # Android doesn't support browser startup timeout.
    return False

  @property
  def device_temp_dir(self) -> device_temp_file.NamedDeviceTemporaryDirectory:
    return self._device_temp_dir

  @property
  def package_name(self) -> str:
    return self._package_name

  @property
  def activity_name(self) -> Optional[str]:
    return None

  @property
  def device(self) -> device_utils.DeviceUtils:
    return self._instance.device

  def _push_seed(self, seed_file: str):
    local_seed_file = posixpath.join(
      self.device_temp_dir.name, os.path.basename(seed_file))
    self.device.adb.Push(seed_file, local_seed_file)

    uid = self.device.GetUidForPackage(self.package_name)
    self.device.RunShellCommand(
      ['chown', uid, local_seed_file], as_root=True)
    return local_seed_file

  #override
  @contextmanager
  def create_driver(
    self,
    seed_file: Optional[str] = None,
    options: Optional[webdriver.ChromeOptions] = None
    ) -> webdriver.Remote:
    options = options or self.default_options
    options.enable_mobile(
      android_package=self.package_name,
      android_activity=self.activity_name,
    )
    # We clean up the application dir and place several files there, so
    # we need to keep the data when running webdriver.
    options.mobile_options['androidKeepAppDataDir'] = True

    if seed_file:
      installed_seed_path = self._push_seed(seed_file)
      logging.info('Installed seed at (%s)', installed_seed_path)
      options.add_argument(
        f'variations-test-seed-path={installed_seed_path}')
      options.add_argument(f'--fake-variations-channel={self.channel}')

    driver = None
    try:
      yield (driver := webdriver.Chrome(service=self.get_driver_service(),
                                        options=options))
    finally:
      if driver:
        driver.quit()

  #override
  def close(self):
    self._instance.Stop()


@attr.attrs()
class WebviewDriverFactory(AndroidDriverFactory):

  #override
  @property
  def package_name(self):
    return 'org.chromium.webview_shell'

  #override
  @property
  def activity_name(self):
    return '.WebViewBrowserActivity'

  #override
  def _install_package(self):
    # Clear the system webview shell.
    self.device.ClearApplicationState(self.package_name)

    ver = android.install_webview(self.channel, self.device)
    logging.info('Installed webview (%s)', ver)

    # Launch shell once to create local state files.
    self.device.StartActivity(
        intent.Intent(
            action='android.intent.action.MAIN',
            package=self.package_name,
            activity='.WebViewBrowserActivity'),
        blocking=True)
    self.device.ForceStop(self.package_name)

  #override
  def _push_seed(self, seed_file: str):
    # Variation seeds for webview are always being loaded from app_webview.
    package_dir = self.device.GetApplicationDataDirectory(self.package_name)
    app_data_dir = posixpath.join(package_dir, 'app_webview')
    local_seed_file = super()._push_seed(seed_file)

    seed_path = posixpath.join(app_data_dir, 'variations_seed')
    seed_new_path = posixpath.join(app_data_dir, 'variations_seed_new')
    self.device.RunShellCommand(
      ['cp', local_seed_file, seed_path], check_return=True, as_root=True)
    self.device.RunShellCommand(
      ['cp', local_seed_file, seed_new_path], check_return=True, as_root=True)
    return local_seed_file
