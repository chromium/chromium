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
class UrlAllowlistTest(ChromeEnterpriseTestCase):
  """Test the URLAllowlist policy.

  This policy provides exceptions to the URLBlocklist policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=URLBlocklist
  and https://cloud.google.com/docs/chrome-enterprise/policies/?policy=URLAllowlist"""

  @before_all
  def setup(self):
    client = self.win_config['client']
    dc = self.win_config['dc']
    self.InstallChrome(client)
    self.InstallWebDriver(client)

    # Blocklist all sites and add an exception with URLAllowlist.
    self.SetPolicy(dc, r'URLBlocklist\1', '*', 'String')
    self.SetPolicy(dc, r'URLAllowlist\1', 'https://google.com', 'String')
    self.RunCommand(client, 'gpupdate /force')

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
  def test_AllowedUrlCanVisit(self):
    output = self.openPage('https://google.com')
    self.assertNotIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

  @test
  def test_NotAllowedUrlCantVisit(self):
    output = self.openPage('https://youtube.com')
    self.assertIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

  @test
  def test_AllowedUrlCanVisitIncognito(self):
    output = self.openPage('https://google.com', incognito=True)
    self.assertNotIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)

  @test
  def test_NotAllowedUrlCantVisitIncognito(self):
    output = self.openPage('https://youtube.com', incognito=True)
    self.assertIn(_ERR_BLOCKED_BY_ADMINISTRATOR, output)
