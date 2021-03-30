# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests generated updater branding details."""

import typ

from test.integration_tests.updater import branding_info

class UpdaterBrandingInfoTest(typ.TestCase):

  def test_company_info(self):
    (self.assertIsNotNone(branding_info.COMPANY_FULLNAME_STRING)
     and self.assertIsNotNone(branding_info.COMPANY_SHORTNAME_STRING))

  def test_product_info(self):
    self.assertIsNotNone(branding_info.PRODUCT_FULLNAME_STRING)

  def test_official_build_str(self):
     self.assertIsNotNone(branding_info.OFFICIAL_BUILD_STRING)

  def test_browser_name_str(self):
     self.assertIsNotNone(branding_info.BROWSER_NAME_STRING)
