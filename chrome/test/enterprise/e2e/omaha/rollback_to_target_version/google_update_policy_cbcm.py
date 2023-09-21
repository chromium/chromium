# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

import packaging.version
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@category('chrome_only')
@environment(file="../policy_test.asset.textpb")
class GoogleUpdatePolicyCloud(ChromeEnterpriseTestCase):
  """Test the Google Update policy:

    https://admx.help/?Category=GoogleUpdate&Policy=Google.Policies.Update::Pol_UpdatePolicyGoogleUrduInput
    """

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'], system_level=True)
    self.InstallGoogleUpdater(self.win_config['client'])
    self.WakeGoogleUpdater(self.win_config['client'])

  @test
  def test_google_update_policy_enabled_cbcm(self):
    # Get chrome version before rollback
    chrome_version_start = packaging.version.parse(
        self.GetChromeVersion(self.win_config['client']))

    # OU: Omaha->Rollback @chromepizzatest.com
    token = '7f8385ea-f21a-431e-91d2-92f994a4a90a'
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    dir = os.path.dirname(os.path.abspath(__file__))
    # Launch Chrome and trigger the cloud enrollment and policy fetch.
    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(dir, 'google_update_policy_webdriver.py'))

    # Trigger Google Updater via Task Scheduler
    self.RunGoogleUpdaterTaskSchedulerCommand(self.win_config['client'],
                                              'Start-ScheduledTask')
    self.WaitForUpdateCheck(self.win_config['client'])

    chrome_version_end = packaging.version.parse(
        self.GetChromeVersion(self.win_config['client']))

    self.assertTrue(chrome_version_start > chrome_version_end)
