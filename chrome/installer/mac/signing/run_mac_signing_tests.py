#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, *((os.pardir,) * 4)))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if TYP_DIR not in sys.path:
    sys.path.insert(0, TYP_DIR)

import typ  # noqa: E402

sys.exit(
    typ.main(top_level_dir=os.path.dirname(THIS_DIR), suffixes=['*_test.py'])
)
