#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the WebDriver Java acceptance tests."""

import optparse
import os
import re
import stat
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(1, os.path.join(_THIS_DIR, os.pardir))

import chrome_paths
import test_environment
import util
import glob

if util.IsLinux():
  sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'build', 'android'))
  from pylib import constants


def _Run(java_tests_src_dir, test_filter, ready_to_run_tests,
         chromedriver_path, chrome_path, log_path, android_package_key,
         debug, tests_report_file):
  """Run the WebDriver Java tests and return the test results.

  Args:
    java_tests_src_dir: the java test source code directory.
    test_filter: the filter to use when choosing tests to run. Format is same
        as Google C++ Test format.
    readyToRunTests: tests that need to run regardless of
        @NotYetImplemented annotation
    chromedriver_path: path to ChromeDriver exe.
    chrome_path: path to Chrome exe.
    log_path: path to server log.
    android_package_key: name of Chrome's Android package.
    debug: whether the tests should wait until attached by a debugger.
  """

  sys_props = ['selenium.browser=chrome',
               'webdriver.chrome.driver=' + os.path.abspath(chromedriver_path)]
  if chrome_path:
    if util.IsLinux() and android_package_key is None:
      # Workaround for crbug.com/611886 and
      # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1695
      chrome_wrapper_path = os.path.join(java_tests_src_dir,
                                         'chrome-wrapper-no-sandbox')
      with open(chrome_wrapper_path, 'w') as f:
        f.write('#!/bin/sh\n')
        f.write('exec "%s" --no-sandbox --disable-gpu "$@"\n' %
            os.path.abspath(chrome_path))
      st = os.stat(chrome_wrapper_path)
      os.chmod(chrome_wrapper_path, st.st_mode | stat.S_IEXEC)
    elif util.IsMac():
      # Use srgb color profile, otherwise the default color profile on Mac
      # causes some color adjustments, so screenshots have unexpected colors.
      chrome_wrapper_path = os.path.join(java_tests_src_dir, 'chrome-wrapper')
      with open(chrome_wrapper_path, 'w') as f:
        f.write('#!/bin/sh\n')
        f.write('exec "%s" --force-color-profile=srgb "$@"\n' %
            os.path.abspath(chrome_path))
      st = os.stat(chrome_wrapper_path)
      os.chmod(chrome_wrapper_path, st.st_mode | stat.S_IEXEC)
    else:
      chrome_wrapper_path = os.path.abspath(chrome_path)
    sys_props += ['webdriver.chrome.binary=' + chrome_wrapper_path]
  if log_path:
    sys_props += ['webdriver.chrome.logfile=' + log_path]
  if android_package_key:
    android_package = constants.PACKAGE_INFO[android_package_key].package
    sys_props += ['webdriver.chrome.android_package=' + android_package]
    if android_package_key == 'chromedriver_webview_shell':
      android_activity = constants.PACKAGE_INFO[android_package_key].activity
      android_process = '%s:main' % android_package
      sys_props += ['webdriver.chrome.android_activity=' + android_activity]
      sys_props += ['webdriver.chrome.android_process=' + android_process]
  if test_filter:
    # Test jar actually takes a regex. Convert from glob.
    test_filter = test_filter.replace('*', '.*')
    sys_props += ['filter=' + test_filter]
  if ready_to_run_tests:
    sys_props += ['readyToRun=' + ready_to_run_tests]

  jvm_args = []
  if debug:
    transport = 'dt_socket'
    if util.IsWindows():
      transport = 'dt_shmem'
    jvm_args += ['-agentlib:jdwp=transport=%s,server=y,suspend=y,'
                 'address=33081' % transport]

  _RunTest(java_tests_src_dir, jvm_args, sys_props, tests_report_file)

def _RunTest(java_tests_src_dir, jvm_args, sys_props, tests_report_file):
  """Runs a single JUnit test suite.

  Args:
    java_tests_src_dir: the directory to run the tests in.
    sys_props: Java system properties to set when running the tests.
    jvm_args: Java VM command line args to use.
  """

  classpath = []
  for name in glob.glob(java_tests_src_dir + "/jar/*.jar"):
    classpath.append(name)

  if util.IsWindows():
    separator = ';'
  else:
    separator = ':'

  code = util.RunCommand(
                         ['java'] +
                         ['-D%s' % sys_prop for sys_prop in sys_props] +
                         ['-D%s' % jvm_arg for jvm_arg in jvm_args] +
                         ['-cp', separator.join(classpath),
                          'org.junit.runner.JUnitCore',
                          'org.openqa.selenium.chrome.ChromeDriverTests'],
                         java_tests_src_dir,
                         tests_report_file)

  if code != 0:
    print 'FAILED to run java tests of ChromeDriverTests'

def _PrintTestResults(results_path):
  """Prints the given results in a format recognized by the buildbot."""
  with open(results_path, "r") as myfile:
    contents = myfile.read()

  successJunitTestsCount = re.search(r'OK \((\d* tests)', contents)

  if successJunitTestsCount:
    testsCount = re.findall(r'INFO: <<< Finished (.*)\)', contents)
    print("Ran %s tests " % len(testsCount))
    myfile.close()
    return 0

  print("============================")
  print("FAILURES DETAILS")
  print("============================")
  start = 'There w'
  end = 'FAILURES!!!'
  print contents[contents.find(start):contents.rfind(end)]

  print("============================")
  print("SUMMARY")
  print("============================")
  testsCount = re.findall(r'INFO: <<< Finished (.*)\)', contents)
  print("Ran %s tests " % len(testsCount))

  failuresCount = re.search(r'There w.* (.*) failure', contents)
  if failuresCount:
    print("Failed %s tests" % failuresCount.group(1))
  failedTests = re.findall(r'\s\d*\) (.*org.openqa.*)', contents)
  testsToReRun = []
  for test in failedTests:
    testName = test.split('(')[0]
    testClass = test.split('(')[1].split('.')[-1]
    testsToReRun.append(testClass[0:-1] + '.' + testName)
  print 'Rerun failing tests with filter: ' + ':'.join(testsToReRun)

  myfile.close()
  return failuresCount.group(1)

def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--verbose', action='store_true', default=False,
      help='Whether output should be verbose')
  parser.add_option(
      '', '--debug', action='store_true', default=False,
      help='Whether to wait to be attached by a debugger')
  parser.add_option(
      '', '--chromedriver', type='string', default=None,
      help='Path to a build of the chromedriver library(REQUIRED!)')
  parser.add_option(
      '', '--chrome', type='string', default=None,
      help='Path to a build of the chrome binary')
  parser.add_option(
      '', '--log-path',
      help='Output verbose server logs to this file')
  parser.add_option(
      '', '--android-package', help='Android package key')
  parser.add_option(
      '', '--filter', type='string', default=None,
      help='Filter for specifying what tests to run, "*" will run all. E.g., '
           '*testShouldReturnTitleOfPageIfSet')
  parser.add_option(
      '', '--also-run-disabled-tests', action='store_true', default=False,
      help='Include disabled tests while running the tests')
  parser.add_option(
      '', '--isolate-tests', action='store_true', default=False,
      help='Relaunch the jar test harness after each test')
  options, _ = parser.parse_args()

  options.chromedriver = util.GetAbsolutePathOfUserPath(options.chromedriver)
  if options.chromedriver is None or not os.path.exists(options.chromedriver):
    parser.error('chromedriver is required or the given path is invalid.' +
                 'Please run "%s --help" for help' % __file__)

  if options.android_package:
    if options.android_package not in constants.PACKAGE_INFO:
      parser.error('Invalid --android-package')
    environment = test_environment.AndroidTestEnvironment(
        options.android_package)
  else:
    environment = test_environment.DesktopTestEnvironment()

  try:
    environment.GlobalSetUp()
    # Run passed tests when filter is not provided.
    if options.isolate_tests:
      test_filters = environment.GetPassedJavaTests()
    else:
      if options.filter:
        test_filter = options.filter
      else:
        test_filter = '*'
      if not options.also_run_disabled_tests:
        if '-' in test_filter:
          test_filter += ':'
        else:
          test_filter += '-'
        test_filter += ':'.join(environment.GetDisabledJavaTestMatchers())
      test_filters = [test_filter]
    ready_to_run_tests = ':'.join(environment.GetReadyToRunJavaTestMatchers())

    java_tests_src_dir = os.path.join(chrome_paths.GetSrc(), 'chrome', 'test',
                                      'chromedriver', 'third_party',
                                      'java_tests')
    tests_report_file = os.path.join(java_tests_src_dir, 'results.txt')

    if (not os.path.exists(java_tests_src_dir) or
        not os.listdir(java_tests_src_dir)):
      java_tests_url = ('https://chromium.googlesource.com/chromium/deps'
                        '/webdriver')
      print ('"%s" is empty or it doesn\'t exist. ' % java_tests_src_dir +
             'Need to map ' + java_tests_url + ' to '
             'chrome/test/chromedriver/third_party/java_tests in .gclient.\n'
             'Alternatively, do:\n'
             '  $ cd chrome/test/chromedriver/third_party\n'
             '  $ git clone %s java_tests' % java_tests_url)
      return 1

    _Run(
      java_tests_src_dir=java_tests_src_dir,
      test_filter=test_filter,
      ready_to_run_tests=ready_to_run_tests,
      chromedriver_path=options.chromedriver,
      chrome_path=util.GetAbsolutePathOfUserPath(options.chrome),
      log_path=options.log_path,
      android_package_key=options.android_package,
      debug=options.debug,
      tests_report_file=tests_report_file)
    return _PrintTestResults(tests_report_file)
  finally:
    environment.GlobalTearDown()
    if(os.path.exists(tests_report_file)):
     os.remove(tests_report_file)
    if(os.path.exists(os.path.join(java_tests_src_dir,
                                   "chrome-wrapper-no-sandbox"))):
      os.remove(os.path.join(java_tests_src_dir, "chrome-wrapper-no-sandbox"))
    if(os.path.exists(os.path.join(java_tests_src_dir, "chrome-wrapper"))):
      os.remove(os.path.join(java_tests_src_dir, "chrome-wrapper"))

if __name__ == '__main__':
  sys.exit(main())
