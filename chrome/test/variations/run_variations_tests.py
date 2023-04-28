#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pytest
import sys

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))

sys.path.append(SRC_DIR)
from testing.scripts import common

if __name__ == "__main__":
  parser = argparse.ArgumentParser()

  # Ignore those arguments for now
  parser.add_argument('--git-revision',
                      '--gerrit-issue',
                      '--gerrit-patchset',
                      '--buildbucket-id')

  parser.add_argument('--isolated-script-test-output',
                      '--write-full-results-to',
                      '--json-results-file',
                      dest='json_results_file',
                      help='If present, store test results on this path.')

  parser.add_argument('--pytest-path',
                      default=os.path.abspath(os.path.dirname(__file__)),
                      dest='pytest_path',
                      help='The path to a test file or a test directory. '
                      'Defaults to the current directory.')

  args, unknown_args = parser.parse_known_args()

  retcode = pytest.main(["-qq", "-s", args.pytest_path, *unknown_args])
  if args.json_results_file:
    with open(args.json_results_file, 'w') as f:
      common.record_local_script_results('variations_smoke_tests', f, [],
                                         retcode == 0)
  sys.exit(retcode)
