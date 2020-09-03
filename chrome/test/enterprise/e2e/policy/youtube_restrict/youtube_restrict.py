# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class YouTubeRestrictTest(ChromeEnterpriseTestCase):
  """Test the ForceYouTubeRestrict policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ForceYouTubeRestrict"""

  RestrictedText = "Restricted Mode is enabled by your network administrator"

  @before_all
  def setup(self):
    self.InstallChrome('client2019')
    self.InstallWebDriver('client2019')

  def openRestrictedVideo(self):
    url = "https://www.youtube.com/results?search_query=restricted"
    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunWebDriverTest('client2019',
                                   os.path.join(dir, '../open_page.py'),
                                   ['--url', url, '--wait=5', '--text_only'])
    return output

  @test
  def test_UnrestrictedYouTubeCanWatchVideo(self):
    self.SetPolicy('win2019-dc', 'ForceYouTubeRestrict', 0, 'DWORD')
    self.RunCommand('client2019', 'gpupdate /force')

    output = self.openRestrictedVideo()
    self.assertNotIn(YouTubeRestrictTest.RestrictedText, output)

  @test
  def test_StrictRestrictedYouTubeCantWatchVideo(self):
    self.SetPolicy('win2019-dc', 'ForceYouTubeRestrict', 2, 'DWORD')
    self.RunCommand('client2019', 'gpupdate /force')

    output = self.openRestrictedVideo()
    self.assertIn(YouTubeRestrictTest.RestrictedText, output)
