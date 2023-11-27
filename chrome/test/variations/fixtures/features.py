# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
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
                 add_tag: result_sink.AddTag) -> AddFeatures:
  """Logs features for the current test."""
  def _add_features_fn(features: Features):
    for feature in features.enabled:
      add_tag('enabled_feature', feature)
    for feature in features.disabled:
      add_tag('disabled_feature', feature)
  return _add_features_fn
