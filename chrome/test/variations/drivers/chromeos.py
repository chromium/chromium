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


CACHE_DIR = os.path.join(SRC_DIR, "build", "cros_cache")

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
  b_options.browser_startup_timeout = 15
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
  board: str = attr.attrib()
  server_port: int = attr.attrib()

  #override
  def __attrs_post_init__(self):
    super().__attrs_post_init__()
    # We use this to check whether we have started the VM before we attempt to
    # shut it down.
    self._vm_started = False

  def _launch_vm(self) -> vm.VM:
    parser = vm.VM.GetParser()
    opts = parser.parse_args([
      f'--board={self.board}',
      f'--cache-dir={CACHE_DIR}',
    ])
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
    remote_device = self.device.remote
    assert remote_device.IsDirWritable('/tmp/'), 'tmp dir not writable'
    remote_device.CopyToDevice(src=seed_file,
                               dest=remote_seed_path,
                               mode='scp',
                               verbose=True)
    assert remote_device.IfFileExists(remote_seed_path), (
      'file not pushed to device'
    )

    # The default owner is root, we need to chmod to any user.
    remote_device.run(
      ['chmod', 'a+rw', remote_seed_path], remote_sudo=True, print_cmd=True)
    return remote_seed_path

  #override
  @property
  def supports_startup_timeout(self) -> bool:
    # ChromeOS is a remote driver that doesn't support browser startup timeout.
    return False

  @functools.cached_property
  def device(self) -> device.Device:
    device_ = self._launch_vm()
    self._vm_started = True
    return device_

  @property
  def vm_started(self) -> bool:
    return self._vm_started

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
    # This has a side-effect to boot up the VM if not yet already.
    assert self.device, "VM fails to boot."

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

    options = options or self.default_options
    options.debugger_address=f'localhost:{debugging_port}'

    with self.tunnel_context(debugging_port, self.server_port):
      driver = webdriver.Chrome(service=self.get_driver_service(),
                                options=options)
      # VM may not be fully ready before it returns, wait for window handle
      # to double confirm.
      self.wait_for_window(driver)
      try:
        yield driver
      finally:
        driver.quit()

  def close(self):
    if self.vm_started and self.device.IsRunning():
      self.device.Stop()
    pass
