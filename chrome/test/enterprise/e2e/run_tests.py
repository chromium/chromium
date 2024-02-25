# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import sys
from chrome_ent_test.infra.multi import ArgsParser, SimpleHostProvider, SharedHostProvider, MultiTestController
import traceback
import warnings

# Import all known tests
from connector import *
from omaha import *
from policy import *


def ParseArgs():
  example = '%s --hosts ./hosts/' % sys.argv[0]

  parser = argparse.ArgumentParser(
      description='Test suite runner for CELab', epilog='example: %s' % example)

  parser.add_argument(
      '--hosts',
      required=True,
      metavar='<host_file;...>',
      help='Full paths to *.host.textpb files to use for tests (or directory)')
  parser.add_argument(
      '--tests',
      metavar='<test_class;...>',
      default='*',
      help='Fully qualified names of TestCases to run (supports my.package.*)')
  parser.add_argument(
      '--include',
      metavar='<categoryA;...>',
      default=None,
      help='Categories of tests to include')
  parser.add_argument(
      '--exclude',
      metavar='<categoryA;...>',
      default=None,
      help='Categories of tests to exclude')
  parser.add_argument(
      '--noprogress',
      dest='show_progress',
      default=True,
      action='store_false',
      help='Don\'t show progress while running tests')
  parser.add_argument(
      '--test_py',
      dest='test_py',
      default=os.path.join('test.py'),
      help='Path to the script to use to launch a single test')
  parser.add_argument(
      '--test_py_args',
      dest='test_py_args',
      default=None,
      help='Arguments to pass to the --test_py script.')
  parser.add_argument(
      '--shared_provider_storage',
      metavar='<bucketName>',
      dest='shared_provider_storage',
      default=None,
      action='store',
      help='Where to store locks for the SharedHostProvider hosts')
  parser.add_argument(
      '--error_logs_dir',
      metavar='<path>',
      dest='error_logs_dir',
      default=None,
      action='store',
      help='Where to collect extra logs on test failures')
  parser.add_argument(
      '-v',
      '--verbosity',
      dest='verbosity',
      default=-1,
      help='Logging verbosity level. Messages logged at this level or lower' +
      'will be included. Set to 1 for debug logging.')

  return parser.parse_args()


def ConfigureLogging(args):
  level = logging.WARNING
  if args.verbosity == "0":
    level = logging.INFO
  if args.verbosity == "1":
    level = logging.DEBUG

  # Filter out logs from low level loggers
  errorOnlyLoggers = ['googleapiclient', 'google.auth', 'google_auth_httplib2']
  for logger in errorOnlyLoggers:
    logging.getLogger(logger).setLevel(logging.ERROR)
  message = 'We recommend that most server applications use service accounts.'
  warnings.filterwarnings('ignore', '.*%s' % message)

  logfmt = '%(asctime)s %(filename)s:%(lineno)s: [%(levelname)s] %(message)s'
  datefmt = '%Y/%m/%d %H:%M:%S'

  logging.basicConfig(level=level, format=logfmt, datefmt=datefmt)

  logging.error("%s: Logging level error is visible." % __file__)
  logging.warning("%s: Logging level warning is visible." % __file__)
  logging.info("%s: Logging level info is visible." % __file__)
  logging.debug("%s: Logging level debug is visible." % __file__)


if __name__ == '__main__':
  args = ParseArgs()

  ConfigureLogging(args)

  logging.info("Arguments: %s" % args)

  tests = ArgsParser.ParseTestsArg(args.tests)
  logging.debug('Found tests: %s', tests)

  if args.include or args.exclude:
    tests = ArgsParser.ProcessTestFilterArg(tests, args.include, args.exclude)
    logging.debug('Got filtered tests: %s', tests)

  hostFiles = ArgsParser.ParseHostsArg(args.hosts)
  logging.debug('Found hosts: %s', hostFiles)

  hostProvider = None
  if args.shared_provider_storage == None:
    hostProvider = SimpleHostProvider(hostFiles)
  else:
    hostProvider = SharedHostProvider(hostFiles, args.shared_provider_storage)

  test_env = None
  if 'LUCI_CONTEXT' in os.environ:
    with open(os.environ['LUCI_CONTEXT']) as f:
      test_env = json.load(f)
  c = MultiTestController(
      tests, hostProvider, args.error_logs_dir, environ=test_env)

  success = False
  try:
    success = c.ExecuteTestCases(args.test_py, args.test_py_args,
                                 args.show_progress)
  except KeyboardInterrupt:
    logging.error('Test run aborted.')
    should_write_logs = False
  except:
    print(traceback.format_exc())
    logging.error('Test run failed.')

  sys.exit(0 if success else 1)
