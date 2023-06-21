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
class YouTubeRestrictTest(ChromeEnterpriseTestCase):
  """Test the ForceYouTubeRestrict policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ForceYouTubeRestrict"""

  RestrictedText = "Restricted Mode is enabled by your network administrator"

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def openRestrictedVideo(self):
    url = "https://www.youtube.com/results?search_query=restricted"
    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(dir, './youtube_restrict_webdriver.py'),
        ['--url', url, '--wait=5'])
    return output

  @test
  def test_UnrestrictedYouTubeCanWatchVideo(self):
    self.SetPolicy(self.win_config['dc'], 'ForceYouTubeRestrict', 0, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self.openRestrictedVideo()
    self.assertNotIn(YouTubeRestrictTest.RestrictedText, output)

  @test
  def test_StrictRestrictedYouTubeCantWatchVideo(self):
    self.SetPolicy(self.win_config['dc'], 'ForceYouTubeRestrict', 2, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    output = self.openRestrictedVideo()
    self.assertIn(YouTubeRestrictTest.RestrictedText, output)
