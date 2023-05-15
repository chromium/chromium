# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


pytest_plugins = [
  'chrome.test.variations.fixtures.driver',
  'chrome.test.variations.fixtures.http',
  'chrome.test.variations.fixtures.seed_locator',
  'chrome.test.variations.fixtures.skia_gold',
]
