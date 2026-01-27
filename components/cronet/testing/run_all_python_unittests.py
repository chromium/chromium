#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)

# Help Python find typ in //third_party/catapult/third_party/typ/
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'third_party', 'typ'))
import typ


if __name__ == '__main__':
  sys.exit(typ.main(top_level_dirs=[os.path.join(REPOSITORY_ROOT, 'components', 'cronet')], skip=['tools.api_static_checks_unittest.*']))