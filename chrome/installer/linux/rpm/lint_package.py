#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performs some static analysis checks on Chrome RPM packages using
rpmlint.
"""

import argparse
import os
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

parser = argparse.ArgumentParser()
parser.add_argument('package', help='path/to/package.rpm')
args = parser.parse_args()
package = os.path.abspath(args.package)

cmd = [
    'rpmlint',
    '-f',
    os.path.join(SCRIPT_DIR, 'rpmlint.conf'),
    '-o',
    'CompressExtension \'gz\'',
    str(package),
]
subprocess.check_call(cmd)
