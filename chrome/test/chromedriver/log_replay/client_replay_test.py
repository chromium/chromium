#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the client_replay.py script.

To run tests, just call this script with the intended chrome, chromedriver
binaries and an optional test filter. This file interfaces with the
client_replay mainly using the CommandSequence class. Each of the test cases
matches a test case from chromedriver/test/run_py_tests.py; they run the same
case from run_py_tests.py, then replay the log and check that the behavior
matches.
"""
# pylint: disable=g-import-not-at-top, g-bad-import-order
import json
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
import unittest
import client_replay

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
_TEST_DIR = os.path.join(_PARENT_DIR, "test")
_PY_TESTS = os.path.join(_TEST_DIR, "run_py_tests.py")

sys.path.insert(1, _PARENT_DIR)
import chrome_paths
import util
sys.path.remove(_PARENT_DIR)

sys.path.insert(1, _TEST_DIR)
import unittest_util
import webserver
sys.path.remove(_TEST_DIR)
# pylint: enable=g-import-not-at-top, g-bad-import-order


_NEGATIVE_FILTER = []


def SubstituteVariableEntries(s):
  """Identifies and removes items that can legitimately vary between runs."""
  white_list = r'(("(id|userDataDir|frameId|version' \
               r'|element-6066-11e4-a52e-4f735466cecf|message|timestamp' \
               r'|expiry|chromedriverVersion|sessionId)": ' \
               r'("[0-9]\.[0-9]*(\.[0-9]*)? \([a-f0-9]*\)"|[^\s},]*))' \
               r'|CDwindow-[A-F0-9]*|cd_frame_id_="[a-f0-9]*")'

  return re.sub(white_list, "<variable_item>", s)


def ClearPort(s):
  """Removes port numbers from urls in the given string."""
  s = re.sub(r":([0-9]){5}/", "<port>", s)
  return re.sub(r"localhost:([0-9]*)", "localhost:<port>", s)


def GenerateTestLog(test_name, chromedriver_path, chrome_path, log_dir):
  """Place the ChromeDriver log from running |test_name| in |log_dir|.

  The log file is put in |log_dir| and named |test_name|.log.

  Args:
    test_name: test name from run_py_tests.py. Example: testGetTitle
    chromedriver_path: path to ChromeDriver binary
    chrome_path: path to Chrome binary
    log_dir: directory in which to put the ChromeDriver log file.
  Raises:
    RuntimeError: run_py_tests.py had a test failure or other error.
  """
  args = [
      sys.executable,
      _PY_TESTS,
      "--chromedriver=%s" % chromedriver_path,
      "--chrome=%s" % chrome_path,
      "--replayable=true",
      "--log-path=%s" % log_dir,
      "--filter=%s" % ("*" + test_name)
  ]
  result = subprocess.call(args)
  if result != 0:
    raise RuntimeError("run_py_tests.py could not be run or failed.")


class ChromeDriverClientReplayTest(unittest.TestCase):
  """Base class for test cases."""

  def __init__(self, *args, **kwargs):
    super(ChromeDriverClientReplayTest, self).__init__(*args, **kwargs)

  @classmethod
  def setUpClass(cls):
    """Starts the server for the necessary pages for testing."""
    # make a temp dir to place test logs into
    cls.log_dir = tempfile.mkdtemp()
    try:
      cls.http_server = webserver.WebServer(chrome_paths.GetTestData())
      cls.server_url = cls.http_server.GetUrl()
    except Exception:
      cls.tearDownClass()
      raise

  @classmethod
  def tearDownClass(cls):
    """Tears down the server."""
    shutil.rmtree(cls.log_dir)
    if getattr(cls, 'http_server', False):
      cls.http_server.Shutdown()

  def CheckResponsesMatch(self, real, logged):
    """Asserts that the given pair of responses match.

    These are usually the replay response and the logged response.
    Checks that they match, up to differences in session, window, element
    IDs, timestamps, etc.

    Args:
      real: actual response from running the command
      logged: logged response, taken from the log file
    """
    if not real and not logged:
      return

    if isinstance(real, dict) and "message" in real:
      real = "ERROR " + real["message"].split("\n")[0]

    # pylint: disable=unidiomatic-typecheck
    self.assertTrue(type(logged) == type(real)
                    or (isinstance(real, basestring)
                        and isinstance(logged, basestring)))
    # pylint: enable=unidiomatic-typecheck

    if isinstance(real, basestring) \
        and (real[:14] == "<!DOCTYPE html" or real[:5] == "<html"):
      real = "".join(real.split())
      logged = "".join(logged.split())

    if not isinstance(real, basestring):
      real = json.dumps(real)
      logged = json.dumps(logged)

    real = ClearPort(real)
    logged = ClearPort(logged)
    real = SubstituteVariableEntries(real)
    logged = SubstituteVariableEntries(logged)

    self.assertEqual(real, logged)

  def runTest(self, test_name):
    """Runs the test. Compares output from Chrome to the output in the log file.

    Args:
      test_name: name of the test to run from run_py_tests.py.
    """
    log_file = os.path.join(ChromeDriverClientReplayTest.log_dir,
                            test_name + ".log")
    GenerateTestLog(test_name, _CHROMEDRIVER, _CHROME, log_file)
    with open(log_file) as lf:
      replay_path = log_file if _OPTIONS.devtools_replay else ""
      server = client_replay.StartChromeDriverServer(_CHROMEDRIVER,
                                                     _OPTIONS.output_log_path,
                                                     replay_path)
      chrome_binary = (util.GetAbsolutePathOfUserPath(_CHROME)
                       if _CHROME else None)

      replayer = client_replay.Replayer(lf, server, chrome_binary,
                                        self.server_url)
      real_response = None
      while True:
        command = replayer.command_sequence.NextCommand(real_response)
        if not command:
          break
        logged_response = replayer.command_sequence._last_response
        real_response = replayer.executor.Execute(
            client_replay._COMMANDS[command.name],
            command.GetPayloadPrimitive())

        self.CheckResponsesMatch(real_response["value"],
                                 logged_response.GetPayloadPrimitive())
      server.Kill()

  def GetFunctionName(self):
    """Get the name of the function that calls this one."""
    # https://stackoverflow.com/questions/251464/
    #   how-to-get-a-function-name-as-a-string-in-python
    return traceback.extract_stack(None, 2)[0][2]

  def testGetPageSource(self):
    self.runTest(self.GetFunctionName())

  def testCloseWindow(self):
    self.runTest(self.GetFunctionName())

  def testIFrameWithExtensionsSource(self):
    self.runTest(self.GetFunctionName())

  def testUnexpectedAlertBehaviour(self):
    self.runTest(self.GetFunctionName())

  def testFileDownloadWithClick(self):
    self.runTest(self.GetFunctionName())

  def testCanSwitchToPrintPreviewDialog(self):
    self.runTest(self.GetFunctionName())

  def testClearElement(self):
    self.runTest(self.GetFunctionName())

  def testEmulateNetworkConditions(self):
    self.runTest(self.GetFunctionName())

  def testSwitchToWindow(self):
    self.runTest(self.GetFunctionName())

  def testEvaluateScript(self):
    self.runTest(self.GetFunctionName())

  def testEvaluateInvalidScript(self):
    self.runTest(self.GetFunctionName())

  def testGetTitle(self):
    self.runTest(self.GetFunctionName())

  def testSendCommand(self):
    self.runTest(self.GetFunctionName())

  def testGetSessions(self):
    self.runTest(self.GetFunctionName())

  def testQuitASessionMoreThanOnce(self):
    self.runTest(self.GetFunctionName())


def GetNegativeFilter(chrome_version):
  """Construct the appropriate negative test filter for the chrome ."""
  if _NEGATIVE_FILTER:
    return "*-" + ":__main__.".join([""] + _NEGATIVE_FILTER)
  return "*"


def main():
  usage = "usage: %prog <chromedriver binary> <chrome binary> [options]"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option(
      "", "--output-log-path",
      help="Output verbose server logs to this file")
  parser.add_option(
      "", "--filter", type="string", default="*",
      help="Filter for specifying what tests to run, \"*\" will run all,"
      "including tests excluded by default. E.g., *testRunMethod")
  parser.add_option(
      "", "--devtools-replay", help="Replay DevTools instead of using\n"
      "real Chrome.")

  # Need global to access these from the test runner.
  # pylint: disable=global-variable-undefined
  global _OPTIONS, _CHROMEDRIVER, _CHROME
  # pylint: enable=global-variable-undefined
  _OPTIONS, args = parser.parse_args()
  _CHROMEDRIVER = util.GetAbsolutePathOfUserPath(args[0])
  _CHROME = util.GetAbsolutePathOfUserPath(args[1])
  if not os.path.exists(_CHROMEDRIVER):
    parser.error("Path given for chromedriver is invalid.\n"
                 'Please run "%s --help" for help' % __file__)
  if not os.path.exists(_CHROME):
    parser.error("Path given for chrome is invalid.\n"
                 'Please run "%s --help" for help' % __file__)

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  test_filter = (GetNegativeFilter()
                 if not _OPTIONS.filter else _OPTIONS.filter)

  tests = unittest_util.FilterTestSuite(all_tests_suite, test_filter)
  result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(tests)
  sys.exit(len(result.failures) + len(result.errors))

if __name__ == "__main__":
  main()