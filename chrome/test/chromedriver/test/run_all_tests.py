#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all ChromeDriver end to end tests."""

import optparse
import os
import platform
import shutil
import sys
import tempfile
import traceback

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
sys.path.insert(0, _PARENT_DIR)

import archive
import chrome_paths
import util

sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'build', 'android'))
from pylib import constants


def _GenerateTestCommand(script,
                         chromedriver,
                         ref_chromedriver=None,
                         chrome=None,
                         chrome_version=None,
                         android_package=None,
                         verbose=False):
  _, log_path = tempfile.mkstemp(prefix='chromedriver_log_')
  print 'chromedriver server log: %s' % log_path
  cmd = [
      sys.executable,
      os.path.join(_THIS_DIR, script),
      '--chromedriver=%s' % chromedriver,
      '--log-path=%s' % log_path,
  ]
  if ref_chromedriver:
    cmd.append('--reference-chromedriver=' + ref_chromedriver)

  if chrome:
    cmd.append('--chrome=' + chrome)

  if chrome_version:
    cmd.append('--chrome-version=' + chrome_version)

  if verbose:
    cmd.append('--verbose')

  if android_package:
    cmd = ['xvfb-run', '-a'] + cmd
    cmd.append('--android-package=' + android_package)
  return cmd


def RunReplayTests(chromedriver, chrome,
                   chrome_version=None, chrome_version_name=None):
  version_info = ''
  if chrome_version_name:
    version_info = '(%s)' % chrome_version_name
  util.MarkBuildStepStart('replay_tests%s' % version_info)

  _, log_path = tempfile.mkstemp(prefix='chromedriver_log_')
  print 'chromedriver server log: %s' % log_path
  cmd = [
    sys.executable,
    os.path.join(_PARENT_DIR, 'log_replay', 'client_replay_test.py'),
    chromedriver,
    chrome,
    '--output-log-path=%s' % log_path
  ]
  if chrome_version:
    cmd.append('--chrome-version=%s' % chrome_version)
  code = util.RunCommand(cmd)

  if code:
    util.MarkBuildStepError()
  return code


def RunPythonTests(chromedriver, ref_chromedriver,
                   chrome=None, chrome_version=None,
                   chrome_version_name=None, android_package=None):
  version_info = ''
  if chrome_version_name:
    version_info = '(%s)' % chrome_version_name
  util.MarkBuildStepStart('python_tests%s' % version_info)
  code = util.RunCommand(
      _GenerateTestCommand('run_py_tests.py',
                           chromedriver,
                           ref_chromedriver=ref_chromedriver,
                           chrome=chrome,
                           chrome_version=chrome_version,
                           android_package=android_package))
  if code:
    util.MarkBuildStepError()
  return code


def RunJavaTests(chromedriver, chrome=None, chrome_version=None,
                 chrome_version_name=None, android_package=None,
                 verbose=False):
  version_info = ''
  if chrome_version_name:
    version_info = '(%s)' % chrome_version_name
  util.MarkBuildStepStart('java_tests%s' % version_info)
  code = util.RunCommand(
      _GenerateTestCommand('run_java_tests.py',
                           chromedriver,
                           ref_chromedriver=None,
                           chrome=chrome,
                           chrome_version=chrome_version,
                           android_package=android_package,
                           verbose=verbose))
  if code:
    util.MarkBuildStepError()
  return code


def RunCppTests(cpp_tests):
  util.MarkBuildStepStart('chromedriver_tests')
  code = util.RunCommand([cpp_tests])
  if code:
    util.MarkBuildStepError()
  return code


def DownloadChrome(version_name, revision, download_site):
  util.MarkBuildStepStart('download %s' % version_name)
  try:
    temp_dir = util.MakeTempDir()
    return (temp_dir, archive.DownloadChrome(revision, temp_dir, download_site))
  except Exception:
    traceback.print_exc()
    util.AddBuildStepText('Skip Java and Python tests')
    util.MarkBuildStepError()
    return (None, None)


def _KillChromes():
  chrome_map = {
      'win': 'chrome.exe',
      'mac': 'Chromium',
      'linux': 'chrome',
  }
  if util.IsWindows():
    cmd = ['taskkill', '/F', '/IM']
  else:
    cmd = ['killall', '-9']
  cmd.append(chrome_map[util.GetPlatformName()])
  util.RunCommand(cmd)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--android-packages',
      help='Comma separated list of application package names, '
           'if running tests on Android.')
  # Option 'chrome-version' is for desktop only.
  parser.add_option(
      '', '--chrome-version',
      help='Version of chrome, e.g., \'HEAD\', \'27\', or \'26\'.'
           'Default is to run tests against all of these versions.'
           'Notice: this option only applies to desktop.')
  options, _ = parser.parse_args()

  exe_postfix = ''
  if util.IsWindows():
    exe_postfix = '.exe'
  cpp_tests_name = 'chromedriver_tests' + exe_postfix
  server_name = 'chromedriver' + exe_postfix

  required_build_outputs = [server_name]
  if not options.android_packages:
    required_build_outputs += [cpp_tests_name]
  try:
    build_dir = chrome_paths.GetBuildDir(required_build_outputs)
  except RuntimeError:
    util.MarkBuildStepStart('check required binaries')
    traceback.print_exc()
    util.MarkBuildStepError()
  constants.SetBuildType(os.path.basename(build_dir))
  print 'Using build outputs from', build_dir

  chromedriver = os.path.join(build_dir, server_name)
  platform_name = util.GetPlatformName()
  if util.IsLinux() and util.Is64Bit():
    platform_name += '64'
  ref_chromedriver = os.path.join(
      chrome_paths.GetSrc(),
      'chrome', 'test', 'chromedriver', 'third_party', 'java_tests',
      'reference_builds',
      'chromedriver_%s%s' % (platform_name, exe_postfix))

  if options.android_packages:
    os.environ['PATH'] += os.pathsep + os.path.join(
        _THIS_DIR, os.pardir, 'chrome')
    code = 0
    for package in options.android_packages.split(','):
      code1 = RunPythonTests(chromedriver,
                             ref_chromedriver,
                             chrome_version_name=package,
                             android_package=package)
      code2 = RunJavaTests(chromedriver,
                           chrome_version_name=package,
                           android_package=package,
                           verbose=True)
      code = code or code1 or code2
    return code
  else:
    versions = {'HEAD': archive.GetLatestRevision()}
    # Linux64 build numbers
    if util.IsLinux():
      versions['71'] = '599034'
      versions['70'] = '587811'
      versions['69'] = '576753'

    # Mac build numbers
    elif util.IsMac():
      versions['71'] = '599028'
      versions['70'] = '587811'
      versions['69'] = '576753'

    # Windows build numbers
    elif util.IsWindows():
      versions['71'] = '598927'
      versions['70'] = '587814'
      versions['69'] = '576753'

    code = 0
    for version, revision in versions.iteritems():
      if options.chrome_version and version != options.chrome_version:
        continue
      download_site = archive.GetDownloadSite()
      version_name = version
      if version_name == 'HEAD':
        version_name = revision
      temp_dir, chrome_path = DownloadChrome(version_name, revision,
                                             download_site)
      if not chrome_path:
        code = 1
        continue
      code1 = RunPythonTests(chromedriver,
                             ref_chromedriver,
                             chrome=chrome_path,
                             chrome_version=version,
                             chrome_version_name='v%s' % version_name)
      code2 = RunJavaTests(chromedriver, chrome=chrome_path,
                           chrome_version=version,
                           chrome_version_name='v%s' % version_name,
                           verbose=True)
      code3 = RunReplayTests(chromedriver,
                             chrome=chrome_path,
                             chrome_version=version,
                             chrome_version_name='v%s' % version_name)
      code = code or code1 or code2 or code3
      _KillChromes()
      shutil.rmtree(temp_dir)
    cpp_tests = os.path.join(build_dir, cpp_tests_name)
    return RunCppTests(cpp_tests) or code


if __name__ == '__main__':
  sys.exit(main())
