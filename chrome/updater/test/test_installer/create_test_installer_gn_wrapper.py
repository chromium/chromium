#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# GN runs python script with the native interpreter, but
# `embed_install_scripts.py` needs to run with vpython and thus this wrapper.

import os
import subprocess
import sys

if __name__ == '__main__':
    subprocess.run([
        'vpython3.bat',
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'embed_install_scripts.py')
    ] + sys.argv[1:])
