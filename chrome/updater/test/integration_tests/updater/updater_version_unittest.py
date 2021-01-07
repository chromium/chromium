# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests generated updater version details."""

import typ

from test.integration_tests.updater import version_info

class UpdaterVersionInfoTest(typ.TestCase):

  def test_get_updater_version(self):
    self.assertIsNotNone(version_info.UPDATER_VERSION_STRING)
