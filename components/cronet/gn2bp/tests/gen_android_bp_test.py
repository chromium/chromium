# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import tempfile
from pathlib import Path
import os
import sys
import glob

PARENT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                           os.pardir))

sys.path.insert(0, PARENT_ROOT)
import gen_android_bp


class GenerateAndroidBpTest(unittest.TestCase):

  def test_rust_flags_normalize_success_case(self):
    self.assertDictEqual(
        gen_android_bp.normalize_rust_flags(
            ["--cfg=feature=\"float_roundtrip\""]),
        {'--cfg': {'feature="float_roundtrip"'}})

  def test_rust_flags_normalize_value_without_key(self):
    self.assertDictEqual(
        gen_android_bp.normalize_rust_flags(['-Aunused-imports']),
        {'-Aunused-imports': None})

  def test_rust_flags_complicated_case_success(self):
    self.assertDictEqual(
        gen_android_bp.normalize_rust_flags([
            "-Aunused-imports", "-Cforce-unwind-tables=no",
            "--target=aarch64-linux-android", "--cfg", "feature=X",
            "--cfg=feature2=Y"
        ]), {
            '-Aunused-imports': None,
            '-Cforce-unwind-tables': {'no'},
            '--target': {'aarch64-linux-android'},
            '--cfg': {'feature=X', 'feature2=Y'}
        })

  def test_rust_flags_throw_invalid(self):
    self.assertRaisesRegex(
        ValueError, "Field feature=float_roundtrip does not relate to any key",
        lambda: gen_android_bp.normalize_rust_flags(['feature=float_roundtrip']
                                                    ))

  def test_rust_flags_throw_invalid(self):
    self.assertRaisesRegex(
        ValueError,
        "Could not normalize flag --cfg=feature=A=B as it has multiple equal signs.",
        lambda: gen_android_bp.normalize_rust_flags(['--cfg=feature=A=B']))

  def test_rust_flag_at_symbol(self):
    # @filepath is allowed in rustflags to tell the compiler to write the output
    # to that specific file. We don't support it in AOSP but we need to make sure
    # that it doesn't cause any issue and is safely ignored.
    self.assertDictEqual(gen_android_bp.normalize_rust_flags(['@somefilepath']),
                         {'@somefilepath': None})


if __name__ == '__main__':
  unittest.main()
