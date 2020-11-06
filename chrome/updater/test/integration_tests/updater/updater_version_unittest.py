# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests generated updater version details."""

import typ

from test.integration_tests.updater import version_info

class UpdaterVersionInfoTest(typ.TestCase):

  def test_get_updater_version(self):
    self.assertIsNotNone(version_info.UPDATER_VERSION_STRING)

  def test_company_info(self):
    (self.assertIsNotNone(version_info.COMPANY_FULLNAME_STRING)
     and self.assertIsNotNone(version_info.COMPANY_SHORTNAME_STRING))

  def test_product_info(self):
    self.assertIsNotNone(version_info.PRODUCT_FULLNAME_STRING)

  def test_official_build_str(self):
     self.assertIsNotNone(version_info.OFFICIAL_BUILD_STRING)

  def test_browser_name_str(self):
     self.assertIsNotNone(version_info.BROWSER_NAME_STRING)
