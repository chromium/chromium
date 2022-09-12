# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'tools', 'perf'))
from chrome_telemetry_build import chromium_config

CAST_TEST_DIR = os.path.join(
    CHROMIUM_SRC_DIR, 'chromecast', 'cast_core', 'test')
CONFIG = chromium_config.ChromiumConfig(top_level_dir=CAST_TEST_DIR,
                                        benchmark_dirs=[CAST_TEST_DIR])