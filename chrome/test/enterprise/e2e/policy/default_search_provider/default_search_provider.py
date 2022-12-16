# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase

@environment(file="../policy_test.asset.textpb")
class DefaultSearchProviderTest(ChromeEnterpriseTestCase):
  """Test the DefaultSearchProviderEnabled,
              DefaultSearchProviderName,
              DefaultSearchProviderSearchURL

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=DefaultSearchProviderEnabled
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=DefaultSearchProviderName
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=DefaultSearchProviderSearchURL

    """

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  def _get_search_url(self, instance_name):
    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(
        instance_name,
        os.path.join(local_dir, 'default_search_provider_webdriver.py'))
    return output

  @test
  def test_default_search_provider_bing(self):
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderEnabled', 1,
                   'DWORD')
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderName', 'Bing',
                   'String')
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderSearchURL',
                   '"https://www.bing.com/search?q={searchTerms}"', 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self._get_search_url(self.win_config['client'])
    self.assertIn('www.bing.com', output)

  @test
  def test_default_search_provider_yahoo(self):
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderEnabled', 1,
                   'DWORD')
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderName', 'Yahoo',
                   'String')
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderSearchURL',
                   '"https://search.yahoo.com/search?p={searchTerms}"',
                   'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self._get_search_url(self.win_config['client'])
    self.assertIn('search.yahoo.com', output)

  @test
  def test_default_search_provider_disabled(self):
    self.SetPolicy(self.win_config['dc'], 'DefaultSearchProviderEnabled', 0,
                   'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self._get_search_url(self.win_config['client'])
    self.assertIn('http://anything', output)
