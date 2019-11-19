#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, *((os.pardir,) * 4)))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if not TYP_DIR in sys.path:
    sys.path.insert(0, TYP_DIR)

import typ

sys.exit(
    typ.main(top_level_dir=os.path.dirname(THIS_DIR), suffixes=['*_test.py']))
