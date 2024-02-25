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
class GoogleUpdatePolicyGPO(ChromeEnterpriseTestCase):
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
  def test_google_update_policy_enabled_gpo(self):
    # Get chrome version before rollback
    chrome_version_start = packaging.version.parse(
        self.GetChromeVersion(self.win_config['client']))
    target_version = chrome_version_start.major - 5

    self.SetOmahaPolicy(
        self.win_config['dc'],
        r'"TargetVersionPrefix{8A69D345-D564-463C-AFF1-A69D9E530F96}"',
        str(target_version) + '.', 'String')
    self.SetOmahaPolicy(
        self.win_config['dc'],
        r'"RollbackToTargetVersion{8A69D345-D564-463C-AFF1-A69D9E530F96}"', 1,
        'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Trigger Google Updater via Task Scheduler
    self.RunGoogleUpdaterTaskSchedulerCommand(self.win_config['client'],
                                              'Start-ScheduledTask')
    self.WaitForUpdateCheck(self.win_config['client'])

    chrome_version_end = packaging.version.parse(
        self.GetChromeVersion(self.win_config['client']))
    self.assertEqual(chrome_version_end.major, target_version)
    self.assertTrue(chrome_version_start > chrome_version_end)
