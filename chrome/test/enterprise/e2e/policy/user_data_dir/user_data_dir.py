# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class UserDataDirTest(ChromeEnterpriseTestCase):
  """Test the UserDataDir

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=UserDataDir.

    """

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  @test
  def test_user_data_dir(self):
    user_data_dir = r'C:\Temp\Browser\Google\Chrome\UserData'
    self.SetPolicy(self.win_config['dc'], r'UserDataDir', user_data_dir,
                   'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    logging.info('Updated User data dir to: ' + user_data_dir)

    local_dir = os.path.dirname(os.path.abspath(__file__))
    args = ['--user_data_dir', user_data_dir]
    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(local_dir, 'user_data_dir_webdriver.py'), args)

    # Verify user data dir not existing before chrome launch
    self.assertIn('User data before running chrome is False', output)
    # Verify policy in chrome://policy page
    self.assertIn('UserDataDir', output)
    self.assertIn(user_data_dir, output)
    # Verify profile path in chrome:// version
    self.assertIn("Profile path is " + user_data_dir, output)
    # Verify user data dir folder creation
    self.assertIn('User data dir creation is True', output)
