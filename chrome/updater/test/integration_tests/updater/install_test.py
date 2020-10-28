# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests updater installation."""

import logging

from test.integration_tests.updater import test_base
from test.integration_tests.updater.lib import updater_util


class InstallTest(test_base.UpdaterTestBase):
  """Tests for updater install."""

  def setUp(self):
    super(InstallTest, self).setUp()

  def SetUpMockServer(self):
    pass

  def tearDown(self):
    super(InstallTest, self).tearDown()

  def test_install(self):
    logging.info('Build dir: %s', self.context.build_dir)
    updater_util.Install()


if __name__ == '__main__':
  unittest.main()
