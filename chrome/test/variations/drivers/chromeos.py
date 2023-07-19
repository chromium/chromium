# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import os
import socket
import subprocess
import sys
import threading
import time

from contextlib import closing
from contextlib import contextmanager
from typing import List, Optional

import attr
from chrome.test.variations.drivers import DriverFactory
from chrome.test.variations.test_utils import SRC_DIR
from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.chrome import service

# The module chromite is under third_party and imported relative to its root.
sys.path.append(os.path.join(SRC_DIR, 'third_party'))
# The module catapult/telemetry is under third_party and imported relative to
# its root.
sys.path.append(os.path.join(SRC_DIR, 'third_party', 'catapult', 'telemetry'))

from chromite.lib import device, vm
from chromite.lib import remote_access
from telemetry.core.exceptions import BrowserConnectionGoneException
from telemetry.internal.browser import browser_options
from telemetry.internal.platform import cros_device
from telemetry.internal.platform.cros_platform_backend import CrosPlatformBackend
from telemetry.internal.backends.chrome import cros_browser_finder


class _PossibleCrOSBrowser(cros_browser_finder.PossibleCrOSBrowser):
  """The CrOS browser wrapper to filter out start-up args."""

  #override
  def GetBrowserStartupArgs(self, browser_options):
    startup_args = super().GetBrowserStartupArgs(browser_options)
    removed_args = [
      # This flag disables features from the seed file. We need to remove this
      # flag so the browser can load the seed file correctly.
      '--enable-gpu-benchmarking',
    ]
    return [arg for arg in startup_args if arg not in removed_args]


def _launch_browser(browser_args: List[str]) -> 'Browser':
  finder_options = browser_options.BrowserFinderOptions()
  finder_options.browser_type = 'cros-browser'
  finder_options.verbosity = 2
  finder_options.CreateParser().parse_args(args=[])

  b_options = finder_options.browser_options
  b_options.browser_startup_timeout = 5
  b_options.AppendExtraBrowserArgs(browser_args)

  device = cros_device.CrOSDevice(
    host_name='localhost',
    ssh_port=9222,
    ssh_identity=finder_options.ssh_identity,
    is_local=False)
  platform = CrosPlatformBackend.CreatePlatformForDevice(device, None)

  possibleBrowser = _PossibleCrOSBrowser(
    'cros-chrome', finder_options, platform, is_guest=False)
  possibleBrowser.SetUpEnvironment(b_options)

  try:
    browser = possibleBrowser.Create()
  except BrowserConnectionGoneException as e:
    raise WebDriverException from e
  return browser


def _wait_for_port(
    port: int, host: str='localhost', timeout:float=5) -> bool:
  start_time = time.perf_counter()
  with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as sock:
    while(time.perf_counter() - start_time <= timeout):
      if sock.connect_ex((host, port)) == 0:
        return True
      else:
        time.sleep(0.01)
  return False


@attr.attrs()
class CrOSDriverFactory(DriverFactory):
  channel: str = attr.attrib()
  cros_args: List[str] = attr.attrib()
  server_port: int = attr.attrib()
  chromedriver_path: str = attr.attrib()

  def _launch_vm(self, cros_args: Optional[List[str]]):
    parser = vm.VM.GetParser()
    opts = parser.parse_args(cros_args or [])
    _device = device.Device.Create(opts)

    # VM will usually be started on a test bot already.
    if not _device.IsRunning():
      _device.Start()
    return _device

  def _ssh_forward(self, port: int, server_port: int) -> subprocess.Popen:
    local_pfs = remote_access.PortForwardSpec(local_port=port)
    remote_pfs = remote_access.PortForwardSpec(local_port=server_port)
    tunnel = self.device.remote.agent.CreateTunnel(
      to_local=[local_pfs], to_remote=[remote_pfs])
    if not _wait_for_port(port):
      return None
    return tunnel

  def _copy_seed_file(self, seed_file: str) -> str:
    assert os.path.exists(seed_file)
    remote_seed_path = f'/tmp/{os.path.basename(seed_file)}'
    assert self.device.remote.IsDirWritable('/tmp/'), 'tmp dir not writable'
    self.device.remote.CopyToDevice(src=seed_file,
                                    dest=remote_seed_path,
                                    mode='scp',
                                    verbose=True)
    assert self.device.remote.IfFileExists(remote_seed_path), (
      'file not pushed to device'
    )
    return remote_seed_path

  @functools.cached_property
  def device(self) -> device.Device:
    return self._launch_vm(self.cros_args)

  @contextmanager
  def tunnel_context(self, debugging_port, server_port):
    tunnel = self._ssh_forward(debugging_port, server_port)
    if not tunnel:
      raise WebDriverException(f'Unable to forward port: {debugging_port}')

    def poll():
      stat = tunnel.poll()
      while stat == None:
        stat = tunnel.poll()
    threading.Thread(target=poll).start()

    try:
      yield
    finally:
      tunnel.terminate()

  #override
  @contextmanager
  def create_driver(
    self,
    seed_file: Optional[str] = None,
    options: Optional[webdriver.ChromeOptions] = None
    ):

    browser_args = []
    if seed_file:
      remote_seed_path = self._copy_seed_file(seed_file)
      browser_args.extend([
        f'--variations-test-seed-path="{remote_seed_path}"',
        f'--fake-variations-channel={self.channel}',
        '--disable-variations-safe-mode',
        '--disable-field-trial-config',
      ])

    browser = _launch_browser(browser_args)
    debugging_port, _ = browser._browser_backend._FindDevToolsPortAndTarget()

    options = options or webdriver.ChromeOptions()
    options.debugger_address=f'localhost:{debugging_port}'

    with self.tunnel_context(debugging_port, self.server_port):
      driver = webdriver.Chrome(
        service=service.Service(self.chromedriver_path),
        options=options)
      try:
        yield driver
      finally:
        driver.quit()

  def close(self):
    # We leave the VM up and running and let the runner to clean up.
    pass
