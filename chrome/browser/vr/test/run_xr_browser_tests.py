#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script for running XR browser tests properly.

The XR browser tests require some additional setup to run properly, for example
setting ACLs on Windows in order to allow the sandboxed process to work
properly. This setup can technically be done in the test itself, but can take
a significant amount of time, causing tests to time out.

This could be worked around by changing the test runner being used from
//chrome/test/base/browser_tests_runner.cc to a custom one that performs the
setup outside of any tests, but that duplicates code and could potentially make
the XR tests miss any changes made to the base file.
"""

import argparse
import json
import logging
import os
import subprocess
import sys


# These ACLs allow the app container users (the really long numerical strings)
# to read and execute files in whatever directory they're applied to (in this
# case, the output directory). This is required in order for the isolated XR
# device service to work in a sandboxed process. These would normally be set
# during installation, but have to be manually set for tests since the installer
# isn't used.
ACL_STRINGS = [
  'S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-4240214049-4028510897-3317428798:(OI)(CI)(RX)',
  'S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-316187997-3786583170-1043257646:(OI)(CI)(RX)',
]


DEFAULT_TRACING_CATEGORIES = [
  'blink',
  'cc',
  'cpu_profiler',
  'gpu',
  'xr',
]


def GetTestExecutable():
  test_executable = 'xr_browser_tests_binary'
  if sys.platform == 'win32':
    test_executable += '.exe'
  return test_executable


def ResetACLs(path):
  logging.warning(
      'Setting ACLs on %s to default. This might take a while.', path)
  try:
    # It's normally fine to inherit the ACLs from parents, but in this case,
    # we need to explicitly reset every file via /t.
    _ = subprocess.check_output(
        ['icacls', path, '/reset', '/t'], stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Failed to reset ACLs on path %s', path)
    logging.error('Command output: %s', e.output)
    sys.exit(e.returncode)

def SetupWindowsACLs(acl_dir):
  try:
    existing_acls = subprocess.check_output(
        ['icacls', acl_dir], stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Failed to retrieve existing ACLs for directory %s', acl_dir)
    logging.error('Command output: %s', e.output)
    sys.exit(e.returncode)

  have_reset = False
  for acl in ACL_STRINGS:
    if acl not in existing_acls:
      if not have_reset:
        # Some bots seem to have explicit ACLs set on the isolated input
        # directory. Most still have the default, so it appears to be leftover
        # from some previous thing and no longer actively used. So, if we
        # haven't set our ACLs before on this directory, reset them to default
        # first since the explicit ones can cause issues with ours not being
        # inherited.
        have_reset = True
        ResetACLs(acl_dir)
      try:
        logging.warning(
            'ACL %s not set on output directory. Attempting to set.', acl)
        _ = subprocess.check_output(['icacls', acl_dir, '/grant', '*%s' % acl],
            stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError as e:
        logging.error('Failed to apply ACL %s to directory %s', acl, acl_dir)
        logging.error('Command output: %s', e.output)
        sys.exit(e.returncode)


def SetupTracingIfNecessary(args):
  if not args.tracing_output_directory:
    return []

  tracing_categories = args.tracing_categories or DEFAULT_TRACING_CATEGORIES
  with open(os.path.join(args.tracing_output_directory, 'traceconfig.json'),
      'w') as trace_config:
    json.dump({
      'trace_config': {
        'record_mode': 'record-until-full',
        'included_categories': tracing_categories,
      },
      'startup_duraton': 0,
      'result_directory': args.tracing_output_directory,
    }, trace_config)
    return ['--trace-config-file=%s' % trace_config.name]


def CreateArgumentParser():
  parser = argparse.ArgumentParser(
      description='This is a wrapper script around %s. To view help for that, '
                  'run `%s --help`.' % (
                      GetTestExecutable(), GetTestExecutable()))

  group = parser.add_argument_group(
      title='Tracing',
      description='Arguments related to running the tests with tracing enabled')
  group.add_argument('--tracing-output-directory', type=os.path.abspath,
                     help='The directory to store trace logs in. Setting this '
                          'enables tracing in the test.')
  group.add_argument('--tracing-category', action='append', default=[],
                     dest='tracing_categories',
                     help='Add a tracing category to capture. Can be passed '
                          'multiple times, once for each category. If '
                          'unspecified, defaults to %s'
                          % DEFAULT_TRACING_CATEGORIES)

  return parser


def main():
  parser = CreateArgumentParser()
  args, rest_args = parser.parse_known_args()

  if sys.platform == 'win32':
    # This should be copied into the root of the output directory, so the
    # directory this file is in should be the directory we want to change the
    # ACLs of.
    SetupWindowsACLs(os.path.abspath(os.path.dirname(__file__)))

  tracing_args = SetupTracingIfNecessary(args)

  test_executable = os.path.abspath(
      os.path.join(os.path.dirname(__file__), GetTestExecutable()))
  sys.exit(subprocess.call(
      [test_executable, '--run-through-xr-wrapper-script'] +
      tracing_args + rest_args))

if __name__ == '__main__':
  main()
