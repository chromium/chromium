# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class AppsShortcutEnabledTest(ChromeEnterpriseTestCase):
  """Test the ShowAppsShortcutInBookmarkBar policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ShowAppsShortcutInBookmarkBar"""

  Policy = 'ShowAppsShortcutInBookmarkBar'

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

    # Enable the bookmark bar so we can see the Apps Shortcut that lives there.
    self.SetPolicy(self.win_config['dc'], 'BookmarkBarEnabled', 1, 'DWORD')

  def isAppsShortcutVisible(self, instance):
    local = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance,
                            os.path.join(local, 'is_apps_shortcut_visible.py'))
    return "TRUE" in output

  @test
  def test_AppShortcutEnabled(self):
    self.SetPolicy(self.win_config['dc'], AppsShortcutEnabledTest.Policy, 1,
                   'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    visible = self.isAppsShortcutVisible(self.win_config['client'])
    self.assertTrue(visible)

  @test
  def test_AppShortcutDisabled(self):
    self.SetPolicy(self.win_config['dc'], AppsShortcutEnabledTest.Policy, 0,
                   'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    visible = self.isAppsShortcutVisible(self.win_config['client'])
    self.assertFalse(visible)
