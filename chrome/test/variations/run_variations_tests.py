#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pytest
import sys

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))

sys.path.append(SRC_DIR)
sys.path.append(os.getcwd())
from testing.scripts import common

if __name__ == "__main__":
  parser = argparse.ArgumentParser()

  # Ignore those arguments for now
  parser.add_argument('--git-revision',
                      '--gerrit-issue',
                      '--gerrit-patchset',
                      '--buildbucket-id')

  # These are not currently used but supplied from the test runner, we need to
  # ignore them for now so it will not stop the script.
  parser.add_argument('--isolated-script-test-repeat',
                      '--isolated-script-test-filter',
                      '--isolated-script-test-launcher-retry-limit',
                      '--isolated-script-test-perf-output')

  parser.add_argument('--isolated-script-test-output',
                      '--write-full-results-to',
                      '--json-results-file',
                      dest='json_results_file',
                      help='If present, store test results on this path.')

  parser.add_argument('--root-build-dir',
                      dest='root_build_dir',
                      help='The path to build output directory. It can be '
                      'relative to the source root or the absolute path. '
                      'The path will be added to python search path.')

  parser.add_argument('--pytest-path',
                      default=os.path.abspath(os.path.dirname(__file__)),
                      dest='pytest_path',
                      help='The path to a test file or a test directory. '
                      'Defaults to the current directory.')

  parser.add_argument('--magic-vm-cache',
                      dest='magic_vm_cache',
                      help='Path to the magic CrOS VM cache dir. See the '
                      'comment "magic_cros_vm_cache" in mixins.star for '
                      'more info.')

  args, unknown_args = parser.parse_known_args()

  # Adds the output dir to the search path so the generated files can be
  # imported.
  if args.root_build_dir:
    root_build_dir = args.root_build_dir
    if not os.path.isabs(root_build_dir):
      root_build_dir = os.path.join(SRC_DIR, root_build_dir)
    logging.info('setting root dir:', root_build_dir)
    assert os.path.exists(root_build_dir)
    sys.path.append(os.path.abspath(root_build_dir))

  # Copied from chromeos/test_runner.py, the same logic to activate vm cache.
  # https://crsrc.org/c/build/chromeos/test_runner.py;l=989;drc=32666e4204efdc594c7e3cbaa22f18dbc0966b81
  if args.magic_vm_cache:
    full_vm_cache_path = os.path.join(SRC_DIR, args.magic_vm_cache)
    if os.path.exists(full_vm_cache_path):
      with open(os.path.join(full_vm_cache_path, 'swarming.txt'), 'w') as f:
        f.write('non-empty file to make swarming persist this cache')

  retcode = pytest.main(["-qq", "-s", args.pytest_path, *unknown_args])
  sys.exit(retcode)
