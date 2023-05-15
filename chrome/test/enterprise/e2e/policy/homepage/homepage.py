# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from absl import flags
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class HomepageTest(ChromeEnterpriseTestCase):
  """Test HomepageIsNewTabPage and HomepageLocation policies.

  See:
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=HomepageLocation
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=HomepageIsNewTabPage
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ShowHomeButton
  """

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  def _getHomepageLocation(self, instance_name):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance_name,
                            os.path.join(dir, 'get_homepage_url.py'))
    m = re.search(r"homepage:([^ \r\n]+)", output)
    return m.group(1)

  def _isHomeButtonShown(self, instance_name):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance_name,
                            os.path.join(dir, 'get_home_button.py'))
    return 'home button exists' in output

  @test
  def test_HomepageLocation(self):
    # Test the case where
    # -  HomepageIsNewTabPage is false
    # -  HomepageLocation is set
    # In this case, when a home page is opened, the HomepageLocation is used
    self.SetPolicy(self.win_config['dc'], 'HomepageIsNewTabPage', 0, 'DWORD')
    self.SetPolicy(self.win_config['dc'], 'HomepageLocation',
                   '"http://www.example.com/"', 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # verify the home page is the value of HomepageLocation
    homepage = self._getHomepageLocation(self.win_config['client'])
    self.assertIn("www.example.com", homepage)

  @test
  def test_HomepageIsNewTab(self):
    # Test the case when HomepageIsNewTabPage is true
    # In this case, when a home page is opened, the new tab page will be used.
    self.SetPolicy(self.win_config['dc'], 'HomepageIsNewTabPage', 1, 'DWORD')
    self.SetPolicy(self.win_config['dc'], 'HomepageLocation',
                   '"http://www.example.com/"', 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # verify that the home page is the new tab page.
    homepage = self._getHomepageLocation(self.win_config['client'])

    # The URL of the new tab can be one of the following:
    # - chrome://new-tab-page/
    # - chrome://new-tab-page-third-party/
    if 'new-tab-page' in homepage:
      pass
    else:
      self.fail('homepage url is not new tab: %s' % homepage)

  @test
  def test_ShowHomeButton(self):
    # Test the case when ShowHomeButton is true
    self.SetPolicy(self.win_config['dc'], 'ShowHomeButton', 1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    isHomeButtonShown = self._isHomeButtonShown(self.win_config['client'])
    self.assertTrue(isHomeButtonShown)

    # Test the case when ShowHomeButton is false
    self.SetPolicy(self.win_config['dc'], 'ShowHomeButton', 0, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    isHomeButtonShown = self._isHomeButtonShown(self.win_config['client'])
    self.assertFalse(isHomeButtonShown)
