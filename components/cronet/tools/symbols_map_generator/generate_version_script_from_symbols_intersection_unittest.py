# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os
import re
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                 os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.symbols_map_generator.generate_version_script_from_symbols_intersection as intersection_generator  # pylint: disable=wrong-import-position
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position

SYMBOLS_OBJDUMP = os.path.join(REPOSITORY_ROOT, 'components', 'cronet', 'tools',
                               'symbols_map_generator', 'test_data',
                               'symbols_objdump.txt')


class TestBreakagesJson(unittest.TestCase):

  def test_ensure_parsing_of_undefined_symbols_is_correct(self):
    self.assertEqual(
        intersection_generator.parse_and_filter_objdump_symbols(
            cronet_utils.read_file(SYMBOLS_OBJDUMP),
            filter_fn=intersection_generator.filter_undefined_only),
        set(["EVP_aead_aes_256_cbc_sha1_tls_implicit_iv", "boo"]))

  def test_ensure_parsing_of_defined_symbols_is_correct(self):
    self.assertEqual(
        intersection_generator.parse_and_filter_objdump_symbols(
            cronet_utils.read_file(SYMBOLS_OBJDUMP),
            filter_fn=intersection_generator.filter_anything_but_undefined),
        set(["Java_android_net_http_internal_J_N_Mx3geLfM", "foo"]))


if __name__ == '__main__':
  unittest.main()
