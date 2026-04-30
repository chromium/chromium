# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import json
import os
import pathlib
import pytest

from selenium import webdriver

from typing import Callable, List

from chrome.test.variations.fixtures import result_sink
from chrome.test.variations.fixtures import test_options

@dataclasses.dataclass(frozen=True)
class Features:
  """Features enabled/disabled during a test run."""
  enabled: List[str]
  disabled: List[str]

AddFeatures = Callable[[Features], None]


@pytest.fixture
def add_features(test_options: test_options.TestOptions,
                 add_tag: result_sink.AddTag,
                 add_artifact: result_sink.AddArtifact,
                 tmp_path: pathlib.Path) -> AddFeatures:
  """Logs features for the current test."""

  def _add_features_fn(features: Features):
    features_file = os.path.join(tmp_path, 'features.json')
    with open(features_file, 'w') as f:
      f.write(json.dumps(dataclasses.asdict(features)))
    add_artifact('features.json', features_file)

  return _add_features_fn
