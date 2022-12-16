# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@category("chrome_only")
@environment(file="../policy_test.asset.textpb")
class SafeBrowsingEnabledTest(ChromeEnterpriseTestCase):
  """Test the SafeBrowsingEnabled policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=SafeBrowsingEnabled"""

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  def isSafeBrowsingEnabled(self):
    dir = os.path.dirname(os.path.abspath(__file__))
    return self.RunUITest(
        self.win_config['client'],
        os.path.join(dir, 'safe_browsing_ui_test.py'),
        timeout=600)

  @test
  def test_SafeBrowsingDisabledNoWarning(self):
    self.SetPolicy(self.win_config['dc'], r'SafeBrowsingEnabled', 0, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self.isSafeBrowsingEnabled()
    self.assertIn("RESULTS.unsafe_page: False", output)
    self.assertIn("RESULTS.unsafe_download: False", output)

  @test
  def test_SafeBrowsingEnabledShowsWarning(self):
    self.SetPolicy(self.win_config['dc'], r'SafeBrowsingEnabled', 1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self.isSafeBrowsingEnabled()
    self.assertIn("RESULTS.unsafe_page: True", output)
    self.assertIn("RESULTS.unsafe_download: True", output)
