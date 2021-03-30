# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import shutil
import sys

from test.integration_tests.common import path_finder
path_finder.add_typ_dir_to_sys_path()

import typ

class Context(object):
  def __init__(self, build_dir):
    self.build_dir = build_dir

def copy_file(source, destination):
  shutil.copyfile(source, destination)

def main():
  parser = typ.ArgumentParser()
  parser.add_argument('--build-dir',
                      help='Specifies chromium build directory.')
  parser.add_argument('--target-gen-dir')

  runner = typ.Runner()

  # Set this when using context that will be passed to tests.
  runner.win_multiprocessing = typ.WinMultiprocessing.importable

  runner.parse_args(parser,
                    argv=None,
                    tests=[path_finder.get_integration_tests_dir()])

  # setup logging level
  if runner.args.verbose > 1:
    level = logging.DEBUG
  else:
    level = logging.INFO
  logging.basicConfig(level=level)

  # copy dynamically generated updater version_info.py from
  # target gen directory to
  # //chrome/updater/test/integration_tests/updater so that
  # it can be imported as a module during test runs.
  target_gen_dir_abs_path = os.path.abspath(runner.args.target_gen_dir)
  version_file_path = os.path.join(target_gen_dir_abs_path, 'gen',
                                   'chrome', 'updater', 'version_info.py')
  if os.path.exists(version_file_path):
    dest = os.path.join(path_finder.get_integration_tests_dir(),
                        'updater', 'version_info.py')
    copy_file(version_file_path, dest)
  else:
    logging.info('File not found: %s' % version_file_path)
    return -1

  # copy dynamically generated updater branding_info.py from
  # target gen directory to
  # //chrome/updater/test/integration_tests/updater so that
  # it can be imported as a module during test runs.
  branding_file_path = os.path.join(target_gen_dir_abs_path, 'gen',
                                   'chrome', 'updater', 'branding_info.py')
  if os.path.exists(branding_file_path):
    dest = os.path.join(path_finder.get_integration_tests_dir(),
                        'updater', 'branding_info.py')
    copy_file(branding_file_path, dest)
  else:
    logging.info('File not found: %s' % branding_file_path)
    return -2

  runner.context = Context(runner.args.build_dir)
  return runner.run()[0]

if __name__ == "__main__":
  sys.exit(main())
