# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 4)))

TEST_DATA_DIR = os.path.join(
    SRC_DIR, 'chrome', 'test', 'data', 'variations')
