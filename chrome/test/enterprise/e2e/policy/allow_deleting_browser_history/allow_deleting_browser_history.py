# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class AllowDeletingBrowserHistory(ChromeEnterpriseTestCase):
  """Test the AllowDeletingBrowserHistory policy:

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=AllowDeletingBrowserHistory.
    """

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def allowDeletingBrowserHistoryEnabled(self, instance_name):
    """Returns true if AllowDeletingBrowserHistory is enabled."""
    directory = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(directory,
                     'allow_deleting_browser_history_webdriver_test.py'))
    return 'ENABLED' in output

  @test
  def test_allow_deleting_browser_history_enabled(self):
    self.SetPolicy(self.win_config['dc'], r'AllowDeletingBrowserHistory', 1,
                   'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    policy_enabled = self.allowDeletingBrowserHistoryEnabled(
        self.win_config['client'])
    self.assertTrue(policy_enabled)

  @test
  def test_allow_deleting_browser_history_disabled(self):
    self.SetPolicy(self.win_config['dc'], r'AllowDeletingBrowserHistory', 0,
                   'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    policy_enabled = self.allowDeletingBrowserHistoryEnabled(
        self.win_config['client'])
    self.assertFalse(policy_enabled)
