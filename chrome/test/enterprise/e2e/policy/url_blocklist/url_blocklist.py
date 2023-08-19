# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase

_ERR_BLOCKED_BY_ADMINISTRATOR = 'is blocked'


@environment(file="../policy_test.asset.textpb")
class UrlBlocklistTest(ChromeEnterpriseTestCase):
  """Test the URLBlocklist policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=URLBlocklist"""

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def openPage(self, url, incognito=False):
    args = ['--url', url, '--text_only']
    if incognito:
      args += ['--incognito']

    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunWebDriverTest(self.win_config['client'],
                                   os.path.join(dir, '../open_page.py'), args)
    return output

  @test
  def test_BlocklistAllCantVisit(self, incognito=False):
    self.SetPolicy(self.win_config['dc'], r'URLBlocklist\1', '*', 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Verify that we can't visit any site.
    output = self.openPage('https://youtube.com/yt/about/', incognito=incognito)
    self.assertIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

    output = self.openPage('https://google.com', incognito=incognito)
    self.assertIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

  @test
  def test_BlocklistYouTubeCantVisit(self, incognito=False):
    self.SetPolicy(self.win_config['dc'], r'URLBlocklist\1',
                   'https://youtube.com', 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Verify that we can't visit YouTube, but can still visit other sites.
    output = self.openPage('https://youtube.com/yt/about/', incognito=incognito)
    self.assertIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

    output = self.openPage('https://google.com', incognito=incognito)
    self.assertNotIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

  @test
  def test_BlocklistAllCantVisitIncognito(self):
    self.test_BlocklistAllCantVisit(incognito=True)

  @test
  def test_BlocklistYouTubeCantVisitIncognito(self):
    self.test_BlocklistYouTubeCantVisit(incognito=True)
