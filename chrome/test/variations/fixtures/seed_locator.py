# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import attr
import pytest

from chrome.test.variations.test_utils import TEST_DATA_DIR
from enum import Enum
from typing import Mapping

_DEFAULT_SEED_PATH=os.path.join(TEST_DATA_DIR, 'variations_seed')

class SeedName(Enum):
  # The default seed that is similar to what end-user would receive.
  DEFAULT = 1
  # The bad seed that enables the feature 'ForceFieldTrialSetupCrashForTesting'
  # intentionally to crash Chrome.
  CRASH = 2

@attr.attrs()
class SeedLocator:
  all_seed_files: Mapping[SeedName, str] = attr.attrib()

  def get_seed(self, name : SeedName = SeedName.DEFAULT):
    seed_file = self.all_seed_files.get(name, None)
    assert os.path.isabs(seed_file), (
      f'{seed_file} for name {name.name} is not an absolute path.'
    )
    assert os.path.exists(seed_file), (
      f'seed file not found for {name.name}, '
      f'all available seeds are: {self.all_seed_files}'
    )
    return seed_file

def pytest_addoption(parser):
  parser.addoption('--seed-file',
                   default=_DEFAULT_SEED_PATH,
                   dest='seed_file',
                   help='The seed file used to run with the test.')

@pytest.fixture
def seed_locator(pytestconfig) -> SeedLocator:
  """Returns the locator that finds a seed file using names.

  Seed files are defined as name-path mapping so the test don't need to worry
  about seed locations and files for different platforms/channels and
  experiments groups.
  """
  return SeedLocator(all_seed_files={
    SeedName.DEFAULT: os.path.abspath(pytestconfig.getoption('seed_file')),
    SeedName.CRASH: os.path.join(TEST_DATA_DIR, 'crash_seed.json'),
  })
