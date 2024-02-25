#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performs some static analysis checks on Chrome debian packages
using lintian.
"""

import argparse
import os
import subprocess


SUPPRESSIONS = [
    # Google Chrome is not software available on a distro by default,
    # so installing to /opt is correct behavior.
    'dir-or-file-in-opt',
    # Distros usually don't like libraries to be statically linked
    # into binaries because it's easier to push a security patch on a
    # single package than to update many packages.  Chromium
    # statically links some libraries anyway.
    'embedded-library',
    # The setuid sandbox is a setuid binary.
    'setuid-binary',
    # Build configurations with is_official_build=false don't compress
    # the packages.
    'uses-no-compression-for-data-tarball',
]


parser = argparse.ArgumentParser()
parser.add_argument('package', help='path/to/package.deb')
args = parser.parse_args()
package = os.path.abspath(args.package)

cmd = [
    'lintian',
    package,
    '--no-tag-display-limit',
    '--pedantic',
    '--suppress-tags',
    ','.join(SUPPRESSIONS)
]
subprocess.check_call(cmd)
