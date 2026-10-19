#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, *((os.pardir,) * 4)))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')
INSTALLER_SIGNING_DIR = os.path.join(
    SRC_DIR, 'chrome', 'installer', 'mac', 'signing'
)

if TYP_DIR not in sys.path:
    sys.path.insert(0, TYP_DIR)

import typ  # noqa: E402


def main():
    # The tests need to be run in a merged environment where
    # chrome/views_examples/mac/signing/ overwrites the base files from
    # chrome/installer/mac/signing/.
    with tempfile.TemporaryDirectory() as tempdir:
        temp_signing_dir = os.path.join(tempdir, 'signing')
        # Only the library is needed. Chrome's own tests exercise the Chrome
        # specific parts/pipeline/config that are overridden here, so they
        # must not be picked up.
        shutil.copytree(
            INSTALLER_SIGNING_DIR,
            temp_signing_dir,
            ignore=shutil.ignore_patterns('*_test.py', 'test_config.py'),
        )

        # Overwrite with chrome/views_examples/mac/signing files, which also
        # provides the tests to run.
        for filename in os.listdir(THIS_DIR):
            if filename.endswith('.py') and filename != os.path.basename(
                __file__
            ):
                src = os.path.join(THIS_DIR, filename)
                dst = os.path.join(temp_signing_dir, filename)
                if os.path.isfile(src):
                    shutil.copy2(src, dst)

        # Run typ with the temporary directory as the top level
        sys.exit(typ.main(top_level_dir=tempdir, suffixes=['*_test.py']))


if __name__ == '__main__':
    main()
