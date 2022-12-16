# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from absl import flags

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class BookmarkBarEnabledTest(ChromeEnterpriseTestCase):
  """Test the BookmarkBarEnabled

    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=BookmarkBarEnabled.

    If this setting is left not set the user can decide to use this function
    or not.
    """

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  def _getUIStructure(self, instance_name):
    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance_name,
                            os.path.join(local_dir, 'bookmarkbar_webdriver.py'))
    return output

  @test
  def test_bookmark_bar_enabled(self):
    # Enable bookmark bar
    self.SetPolicy(self.win_config['dc'], r'BookmarkBarEnabled', 1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    logging.info('Enabled bookmark bar')

    output = self._getUIStructure(self.win_config['client'])
    self.assertIn('Bookmarkbar is found', output)

  @test
  def test_bookmark_bar_disabled(self):
    # Disable bookmark bar
    self.SetPolicy(self.win_config['dc'], r'BookmarkBarEnabled', 0, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    logging.info('Disabled bookmark bar')

    output = self._getUIStructure(self.win_config['client'])
    self.assertIn('Bookmarkbar is missing', output)
