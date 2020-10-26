# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys

from test.integration_tests.common import path_finder
path_finder.add_typ_dir_to_sys_path()

import typ

class Context(object):
  def __init__(self, build_dir):
    self.build_dir = build_dir


def main():
  parser = typ.ArgumentParser()
  parser.add_argument('--build-dir',
                      help='Specifies chromium build directory.')

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

  runner.context = Context(runner.args.build_dir)
  return runner.run()[0]

if __name__ == "__main__":
  sys.exit(main())
