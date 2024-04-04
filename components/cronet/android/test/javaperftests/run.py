#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script runs an automated Cronet performance benchmark.

This script:
1. Sets up "USB reverse tethering" which allow network traffic to flow from
   an Android device connected to the host machine via a USB cable.
2. Starts HTTP and QUIC servers on the host machine.
3. Installs an Android app on the attached Android device and runs it.
4. Collects the results from the app.

Prerequisites:
1. A rooted (i.e. "adb root" succeeds) Android device connected via a USB cable
   to the host machine (i.e. the computer running this script).
2. quic_server has been built for the host machine, e.g. via:
     gn gen out/Release --args="is_debug=false"
     autoninja -C out/Release quic_server
3. cronet_perf_test_apk has been built for the Android device, e.g. via:
     ./components/cronet/tools/cr_cronet.py gn -r
     autoninja -C out/Release cronet_perf_test_apk
4. If "sudo ufw status" doesn't say "Status: inactive", run "sudo ufw disable".
5. sudo apt-get install lighttpd
6. If the usb0 interface on the host keeps losing it's IPv4 address
   (WaitFor(HasHostAddress) will keep failing), NetworkManager may need to be
   told to leave usb0 alone with these commands:
     sudo bash -c "printf \"\\n[keyfile]\
         \\nunmanaged-devices=interface-name:usb0\\n\" \
         >> /etc/NetworkManager/NetworkManager.conf"
     sudo service network-manager restart

Invocation:
./run.py

Output:
Benchmark timings are output by telemetry to stdout and written to
./results.html

"""

import json
import argparse
import os
import shutil
import sys
import tempfile
import time
import urllib.parse

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..', '..'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools', 'perf'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'build', 'android'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'components'))

# pylint: disable=wrong-import-position
from chrome_telemetry_build import chromium_config
from devil.android import device_utils
from devil.android.sdk import intent
from core import benchmark_runner
from cronet.tools import android_rndis_forwarder
from cronet.tools import perf_test_utils
import lighttpd_server
from pylib import constants
from telemetry import android
from telemetry import benchmark
from telemetry import story as story_module
from telemetry.web_perf import timeline_based_measurement
# pylint: enable=wrong-import-position

# pylint: disable=super-with-arguments


def GetDevice():
  devices = device_utils.DeviceUtils.HealthyDevices()
  assert len(devices) == 1
  return devices[0]


class CronetPerfTestAndroidStory(android.AndroidStory):
  # Android AppStory implementation wrapping CronetPerfTest app.
  # Launches Cronet perf test app and waits for execution to complete
  # by waiting for presence of DONE_FILE.

  def __init__(self, device):
    self._device = device
    config = perf_test_utils.GetConfig(device)
    device.RemovePath(config['DONE_FILE'], force=True)
    self.url ='http://sample/?' + urllib.parse.urlencode(config)
    start_intent = intent.Intent(
        package=perf_test_utils.APP_PACKAGE,
        activity=perf_test_utils.APP_ACTIVITY,
        action=perf_test_utils.APP_ACTION,
        # |config| maps from configuration value names to the configured values.
        # |config| is encoded as URL parameter names and values and passed to
        # the Cronet perf test app via the Intent data field.
        data=self.url,
        extras=None,
        category=None)
    super(CronetPerfTestAndroidStory, self).__init__(
        start_intent, name='CronetPerfTest',
        # No reason to wait for app; Run() will wait for results.  By default
        # StartActivity will timeout waiting for CronetPerfTest, so override
        # |is_app_ready_predicate| to not wait.
        is_app_ready_predicate=lambda app: True)

  def Run(self, shared_state):
    while not self._device.FileExists(
        perf_test_utils.GetConfig(self._device)['DONE_FILE']):
      time.sleep(1.0)


class CronetPerfTestStorySet(story_module.StorySet):

  def __init__(self, device):
    super(CronetPerfTestStorySet, self).__init__()
    # Create and add Cronet perf test AndroidStory.
    self.AddStory(CronetPerfTestAndroidStory(device))


class CronetPerfTestMeasurement(
    timeline_based_measurement.TimelineBasedMeasurement):
  # For now AndroidStory's SharedAppState works only with
  # TimelineBasedMeasurements, so implement one that just forwards results from
  # Cronet perf test app.

  def __init__(self, device, options):
    super(CronetPerfTestMeasurement, self).__init__(options)
    self._device = device

  def WillRunStory(self, platform, story=None):
    # Skip parent implementation which doesn't apply to Cronet perf test app as
    # it is not a browser with a timeline interface.
    pass

  def Measure(self, platform, results):
    # Reads results from |RESULTS_FILE| on target and adds to |results|.
    jsonResults = json.loads(self._device.ReadFile(
        perf_test_utils.GetConfig(self._device)['RESULTS_FILE']))
    for test in jsonResults:
      results.AddMeasurement(test, 'ms', jsonResults[test])

  def DidRunStory(self, platform, results):
    # Skip parent implementation which calls into tracing_controller which this
    # doesn't have.
    pass


class CronetPerfTestBenchmark(benchmark.Benchmark):
  # Benchmark implementation spawning off Cronet perf test measurement and
  # StorySet.
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_ANDROID]

  def __init__(self, max_failures=None):
    super(CronetPerfTestBenchmark, self).__init__(max_failures)
    self._device = GetDevice()

  def CreatePageTest(self, options):
    return CronetPerfTestMeasurement(self._device, options)

  def CreateStorySet(self, options):
    return CronetPerfTestStorySet(self._device)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-format', default='html',
                   help='The output format of the results file.')
  parser.add_argument('--output-dir', default=None,
                   help='The directory for the output file. Default value is '
                        'the base directory of this script.')
  args, _ = parser.parse_known_args()
  constants.SetBuildType(perf_test_utils.BUILD_TYPE)
  # Install APK
  device = GetDevice()
  device.EnableRoot()
  device.Install(perf_test_utils.APP_APK)
  # Start USB reverse tethering.
  android_rndis_forwarder.AndroidRndisForwarder(device,
      perf_test_utils.GetAndroidRndisConfig(device))
  # Start HTTP server.
  http_server_doc_root = perf_test_utils.GenerateHttpTestResources()
  config_file = tempfile.NamedTemporaryFile()
  http_server = lighttpd_server.LighttpdServer(http_server_doc_root,
      port=perf_test_utils.HTTP_PORT,
      base_config_path=config_file.name)
  perf_test_utils.GenerateLighttpdConfig(config_file, http_server_doc_root,
                                         http_server)
  assert http_server.StartupHttpServer()
  config_file.close()
  # Start QUIC server.
  quic_server_doc_root = perf_test_utils.GenerateQuicTestResources(device)
  quic_server = perf_test_utils.QuicServer(quic_server_doc_root)
  quic_server.StartupQuicServer(device)
  # Launch Telemetry's benchmark_runner on CronetPerfTestBenchmark.
  # By specifying this file's directory as the benchmark directory, it will
  # allow benchmark_runner to in turn open this file up and find the
  # CronetPerfTestBenchmark class to run the benchmark.
  top_level_dir = os.path.dirname(os.path.realpath(__file__))
  expectations_files = [os.path.join(top_level_dir, 'expectations.config')]
  runner_config = chromium_config.ChromiumConfig(
      top_level_dir=top_level_dir,
      benchmark_dirs=[top_level_dir],
      expectations_files=expectations_files)
  sys.argv.insert(1, 'run')
  sys.argv.insert(2, 'run.CronetPerfTestBenchmark')
  sys.argv.insert(3, '--browser=android-system-chrome')
  sys.argv.insert(4, '--output-format=' + args.output_format)
  if args.output_dir:
    sys.argv.insert(5, '--output-dir=' + args.output_dir)
  benchmark_runner.main(runner_config)
  # Shutdown.
  quic_server.ShutdownQuicServer()
  shutil.rmtree(quic_server_doc_root)
  http_server.ShutdownHttpServer()
  shutil.rmtree(http_server_doc_root)


if __name__ == '__main__':
  main()
