# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WPT WebDriver tests runner."""

import pytest
import os
import argparse
import sys
import json
import tempfile
import time
import logging

_log = logging.getLogger(__name__)

_TEST_DIR = os.path.abspath(os.path.dirname(__file__))
_CHROMEDRIVER_DIR = os.path.join(_TEST_DIR, os.pardir)
SRC_DIR = os.path.join(_CHROMEDRIVER_DIR, os.pardir, os.pardir, os.pardir)
_CLIENT_DIR = os.path.join(_CHROMEDRIVER_DIR, "client")
_SERVER_DIR = os.path.join(_CHROMEDRIVER_DIR, "server")

sys.path.insert(0, _CHROMEDRIVER_DIR)
import util

sys.path.insert(0, _SERVER_DIR)
import server

BLINK_TOOLS_PATH = 'third_party/blink/tools'
BLINK_TOOLS_ABS_PATH = os.path.join(SRC_DIR, BLINK_TOOLS_PATH)

sys.path.insert(0, BLINK_TOOLS_ABS_PATH)
from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.common import CHROMIUM_WPT_DIR
from blinkpy.web_tests.models import test_expectations

WD_CLIENT_PATH = 'blinkpy/third_party/wpt/wpt/tools/webdriver'
WEBDRIVER_CLIENT_ABS_PATH = os.path.join(BLINK_TOOLS_ABS_PATH, WD_CLIENT_PATH)


class TestShard(object):
  def __init__(self, total_shards, shard_index):
    self.total_shards = total_shards
    self.shard_index = shard_index

  def is_matched_test(self, test_path):
    """Determines if a test belongs to the current shard_index.

    Returns:
      A boolean: True if tests in test_dir should be run in the
      current shard index; False otherwise.
    """
    if self.total_shards == 1:
      return True

    return (hash(test_path) % self.total_shards) == self.shard_index

class WebDriverTestResult(object):
  def __init__(self, test_name, test_status, messsage=None):
    self.test_name = test_name
    self.test_status = test_status
    self.message = messsage

def parse_webdriver_expectations(host, port):
  expectations_path = port.path_to_webdriver_expectations_file()
  file_contents = host.filesystem.read_text_file(expectations_path)
  expectations_dict = {expectations_path: file_contents}
  expectations = test_expectations.TestExpectations(
      port, expectations_dict=expectations_dict)
  return expectations

def prepare_filtered_tests(isolated_script_test_filter, finder, shard, port):
    filter_list = isolated_script_test_filter.split('::')
    filtered_tests = [get_relative_subtest_path(
        test, finder, shard, port) for test in filter_list]
    return filter(None, filtered_tests)

def get_relative_subtest_path(external_test_path, finder, shard, port):
    test_name, subtest_suffix = port.split_webdriver_test_name(
        external_test_path)
    abs_skipped_test_path = finder.path_from_web_tests(test_name)
    if not shard.is_matched_test(abs_skipped_test_path):
      return None

    relative_path = os.path.relpath(abs_skipped_test_path)
    relative_subtest_path = port.add_webdriver_subtest_pytest_suffix(
        relative_path, subtest_suffix)
    return relative_subtest_path

def process_skip_list(skipped_tests, results, finder, port, test_path, shard):
  skip_list = []
  abs_test_path = os.path.abspath(test_path)
  for skipped_test in skipped_tests:
    test_name, subtest_suffix = port.split_webdriver_test_name(
        skipped_test)
    abs_path = finder.path_from_web_tests(test_name)
    if not abs_path.startswith(abs_test_path):
      continue

    if not shard.is_matched_test(abs_path):
      continue

    pytest_subtest_path = port.add_webdriver_subtest_pytest_suffix(
        test_name, subtest_suffix)
    skip_list.append(pytest_subtest_path)
    results.append(WebDriverTestResult(
        skipped_test, 'SKIP'))

  return skip_list

class SubtestResultRecorder(object):
  def __init__(self, path, port):
    self.result = []
    self.test_path = path
    self.port = port

  def pytest_runtest_logreport(self, report):
    if report.passed and report.when == "call":
      self.record_pass(report)
    elif report.failed:
      if report.when != "call":
        self.record_error(report)
      else:
        self.record_fail(report)
    elif report.skipped:
      self.record_skip(report)

  def record_pass(self, report):
    self.record(report, "PASS")

  def record_fail(self, report):
    message = report.longrepr.reprcrash.message
    self.record(report, "FAIL", message=message)

  def record_error(self, report):
    # error in setup/teardown
    if report.when != "call":
      message = "error in %s" % report.when
    self.record(report, "FAIL", message)

  def record_skip(self, report):
    self.record(report, "FAIL",
                "In-test skip decorators are disallowed.")

  def record(self, report, status, message=None):
    # location is a (filesystempath, lineno, domaininfo) tuple
    # https://docs.pytest.org/en/3.6.2/reference.html#_pytest.runner.TestReport.location
    test_name = report.location[2]
    output_name = self.port.add_webdriver_subtest_suffix(
        self.test_path, test_name)
    self.result.append(WebDriverTestResult(
        output_name, status, message))

def set_up_config(path_finder, chromedriver_server):
  # These envs are used to create a WebDriver session in the fixture.py file.
  os.environ["WD_HOST"] = chromedriver_server.GetHost()
  os.environ["WD_PORT"] = str(chromedriver_server.GetPort())
  os.environ["WD_CAPABILITIES"] = json.dumps({
      "goog:chromeOptions": {
          "w3c": True,
          "prefs": {
              "profile": {
                  "default_content_setting_values": {
                      "popups": 1
                  }
              }
          },
          "args": [
              "--host-resolver-rules="
              "MAP nonexistent.*.test ~NOTFOUND, "
              "MAP *.test 127.0.0.1"
          ]

      }
  })

  # Port numbers are defined at
  # https://cs.chromium.org/chromium/src/third_party/blink/tools
  # /blinkpy/web_tests/servers/wptserve.py?l=23&rcl=375b34c6ba64
  # 5d00c1413e4c6106c7bb74581c85
  os.environ["WD_SERVER_CONFIG"] = json.dumps({
    "doc_root": path_finder.path_from_chromium_base(CHROMIUM_WPT_DIR),
    "browser_host": "web-platform.test",
    "domains": {"": {"": "web-platform.test",
                     "www": "www.web-platform.test",
                     "www.www": "www.www.web-platform.test",
                     "www1": "www1.web-platform.test",
                     "www2": "www2.web-platform.test"}},
    "ports": {"ws": [9001], "wss": [9444], "http": [8001], "https": [8444]}})

def run_test(path, path_finder, port, skipped_tests=[]):
  abs_path = os.path.abspath(path)
  external_path = path_finder.strip_web_tests_path(abs_path)
  subtests = SubtestResultRecorder(external_path, port)

  skip_test_flag = ['--deselect=' +
                    skipped_test for skipped_test in skipped_tests]
  pytest_args = [path] + skip_test_flag + \
      ['--rootdir=' + path_finder.web_tests_dir()]

  pytest.main(pytest_args, plugins=[subtests])
  return subtests.result

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.description = __doc__
  parser.add_argument(
      '--chromedriver',
      required=True,
      help='Path to chromedriver binary')
  parser.add_argument(
      '--log-path',
      help='Output verbose server logs to this file')
  parser.add_argument(
      '--chrome', help='Path to chrome binary')
  parser.add_argument(
      '--output-dir',
      help='Output directory for misc logs (e.g. wptserve)')
  parser.add_argument(
      '--isolated-script-test-output',
      help='JSON output file used by swarming')
  parser.add_argument(
      '--isolated-script-test-perf-output',
      help='JSON perf output file used by swarming, ignored')
  parser.add_argument(
      '--isolated-script-test-filter',
      help='isolated script filter string with :: separators')
  parser.add_argument(
      '--test-path',
      required=True,
      help='Path to the WPT WebDriver tests')
  parser.add_argument(
      '-v', '--verbose', action='store_true',
      help='log extra details that may be helpful when debugging')

  options = parser.parse_args()
  env = os.environ

  total_shards = 1
  shard_index = 0
  if 'GTEST_TOTAL_SHARDS' in env:
      total_shards = int(env['GTEST_TOTAL_SHARDS'])
  if 'GTEST_SHARD_INDEX' in env:
      shard_index = int(env['GTEST_SHARD_INDEX'])
  test_shard = TestShard(total_shards, shard_index)

  test_results = []
  test_path = options.test_path
  log_level = logging.DEBUG if options.verbose else logging.INFO
  configure_logging(logging_level=log_level, include_time=True)

  host = Host()
  port = host.port_factory.get()
  if options.output_dir:
    port.set_option_default('results_directory', options.output_dir)
  else:
    output_dir = tempfile.mkdtemp('webdriver_tests')
    _log.info('Using a temporary output dir %s', output_dir)
    port.set_option_default('results_directory', output_dir)
  path_finder = PathFinder(host.filesystem)

  # Starts WPT Serve to serve the WPT WebDriver test content.
  port.start_wptserve()

  # WebDriverExpectations stores skipped and failed WebDriver tests.
  expectations = parse_webdriver_expectations(host, port)
  skip_list = expectations.model().get_tests_with_result_type(
      test_expectations.SKIP).copy()
  skipped_tests = process_skip_list(
      skip_list, test_results, path_finder, port, test_path, test_shard)

  options.chromedriver = util.GetAbsolutePathOfUserPath(options.chromedriver)
  if (not os.path.exists(options.chromedriver) and
      util.GetPlatformName() == 'win' and
      not options.chromedriver.lower().endswith('.exe')):
    options.chromedriver = options.chromedriver + '.exe'

  if not os.path.exists(options.chromedriver):
    parser.error('Path given by --chromedriver is invalid.\n' +
                 'Please run "%s --help" for help' % __file__)

  # Due to occasional timeout in starting ChromeDriver, retry once when needed.
  try:
    chromedriver_server = server.Server(options.chromedriver, options.log_path)
  except RuntimeError as e:
    _log.warn('Error starting ChromeDriver, retrying...')
    chromedriver_server = server.Server(options.chromedriver, options.log_path)

  if not chromedriver_server.IsRunning():
    _log.error('ChromeDriver is not running.')
    sys.exit(1)

  set_up_config(path_finder, chromedriver_server)
  start_time = time.time()

  sys.path.insert(0, WEBDRIVER_CLIENT_ABS_PATH)
  try:
    if options.isolated_script_test_filter:
      filtered_tests = prepare_filtered_tests(
          options.isolated_script_test_filter, path_finder, test_shard, port)
      for filter_test in filtered_tests:
        test_results += run_test(filter_test, path_finder, port)

    elif os.path.isfile(test_path):
      test_results += run_test(test_path, path_finder, port, skipped_tests)
    elif os.path.isdir(test_path):
      for root, dirnames, filenames in os.walk(test_path):
        for filename in filenames:
          if '__init__' in filename or not filename.endswith('.py'):
            continue

          test_file = os.path.join(root, filename)

          if not test_shard.is_matched_test(os.path.abspath(test_file)):
            continue
          test_results += run_test(test_file, path_finder, port, skipped_tests)
    else:
      _log.error('%s is not a file nor directory.' % test_path)
      sys.exit(1)
  except KeyboardInterrupt as e:
    _log.error(e)
  finally:
    chromedriver_server.Kill()
    port.stop_wptserve()

  exit_code = 0
  if options.isolated_script_test_output:
    output = {
      'interrupted': False,
      'num_failures_by_type': { },
      'path_delimiter': '.',
      'seconds_since_epoch': start_time,
      'tests': { },
      'version': 3,
    }

    success_count = 0

    for test_result in test_results:
      if expectations.model().has_test(test_result.test_name):
        expected_result = expectations.get_expectations_string(
            test_result.test_name)
        status = test_expectations.TestExpectations.expectation_from_string(
            test_result.test_status)
        is_unexpected = not expectations.matches_an_expected_result(
            test_result.test_name, status)
      else:
        expected_result = 'PASS'
        is_unexpected = (test_result.test_status != expected_result)

      output['tests'][test_result.test_name] = {
        'expected': expected_result,
        'actual': test_result.test_status,
        'is_unexpected': is_unexpected,
      }

      if test_result.message:
        output['tests'][test_result.test_name]['message'] = test_result.message

      if test_result.test_status == 'PASS':
        success_count += 1

      if is_unexpected and test_result.test_status != 'PASS':
        exit_code += 1

    output['num_failures_by_type']['PASS'] = success_count
    output['num_failures_by_type']['SKIP'] = len(skipped_tests)
    output['num_failures_by_type']['FAIL'] = len(
        test_results) - success_count - len(skipped_tests)

    with open(options.isolated_script_test_output, 'w') as fp:
      json.dump(output, fp)

  if exit_code > exit_codes.MAX_FAILURES_EXIT_STATUS:
    _log.warning('num regressions (%d) exceeds max exit status (%d)',
                 exit_code, exit_codes.MAX_FAILURES_EXIT_STATUS)
    exit_code = exit_codes.MAX_FAILURES_EXIT_STATUS

  sys.exit(exit_code)
