# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import pytest

pytest_plugins = [
  'chrome.test.variations.fixtures.cipd',
  'chrome.test.variations.fixtures.driver',
  'chrome.test.variations.fixtures.http',
  'chrome.test.variations.fixtures.result_sink',
  'chrome.test.variations.fixtures.seed_locator',
  'chrome.test.variations.fixtures.skia_gold',
  'chrome.test.variations.fixtures.features',
  'chrome.test.variations.fixtures.test_options'
]


def pytest_addoption(parser: pytest.Parser):
  # These are not currently used but supplied from the test runner, we need to
  # ignore them for now so it will not stop the script.
  parser.addoption('--isolated-script-test-repeat',
                   '--isolated-script-test-filter',
                   '--isolated-script-test-launcher-retry-limit',
                   '--isolated-script-test-perf-output',
                   '--git-revision',
                   '--gerrit-issue',
                   '--gerrit-patchset',
                   '--buildbucket-id',
                   '--logs-dir')

  parser.addoption('--isolated-script-test-output',
                   '--write-full-results-to',
                   '--json-results-file',
                   dest='json_results_file',
                   help='If present, store test results on this path.')

  parser.addoption('--root-build-dir',
                   dest='root_build_dir',
                   help='The path to build output directory. It can be '
                   'relative to the source root or the absolute path. The path '
                   'will be added to python search path.')

  parser.addoption('--magic-vm-cache',
                   dest='magic_vm_cache',
                   help='Path to the magic CrOS VM cache dir. See the comment '
                   '"magic_cros_vm_cache" in mixins.star for more info.')


def pytest_cmdline_main(config: pytest.Config):
  src_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))

  root_build_dir = config.getoption('root_build_dir')
  # Adds the output dir to the search path so the generated files can be
  # imported.
  if root_build_dir:
    if not os.path.isabs(root_build_dir):
      root_build_dir = os.path.join(src_dir, root_build_dir)
    logging.info('setting root dir:', root_build_dir)
    assert os.path.exists(root_build_dir)
    sys.path.append(os.path.abspath(root_build_dir))

  # Copied from chromeos/test_runner.py, the same logic to activate vm cache.
  # https://crsrc.org/c/build/chromeos/test_runner.py;l=989;drc=32666e4204efdc594c7e3cbaa22f18dbc0966b81
  magic_vm_cache = config.getoption('magic_vm_cache')
  if magic_vm_cache:
    full_vm_cache_path = os.path.join(src_dir, magic_vm_cache)
    if os.path.exists(full_vm_cache_path):
      with open(os.path.join(full_vm_cache_path, 'swarming.txt'), 'w') as f:
        f.write('non-empty file to make swarming persist this cache')
