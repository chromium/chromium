#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# GN runs python scripts with the native interpreter, but `sign.py` needs to run
# with `vpython` to be able to run `resedit`, so this wrapper is needed.

import os
import subprocess
import sys

if __name__ == '__main__':
    subprocess.run([
        'vpython3.bat',
        os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sign.py')
    ] + sys.argv[1:])
