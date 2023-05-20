#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

if __name__ == '__main__':
    sys.path.append(os.path.dirname(__file__))
    import signing.driver
    signing.driver.main(sys.argv[1:])
