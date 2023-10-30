# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from infra import ChromeEnterpriseTestCase


@category('chrome_only')
@environment(file='../connector_test.asset.textpb')
class LocalContentAnalysisTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'], system_level=True)
    self.EnableUITest(self.win_config['client'])
    self.EnableDemoAgent(self.win_config['client'])

  @test
  def test_local_content_analysis(self):
    # OU: beyondcorp.bigr.name -> Local Content Analysis Connector OU
    token = 'd47bff6c-c70a-493e-ae97-de789ef6b19d'
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Launch the demo agent and stream its value

    # UI opertaion like pate block, or upload text file with block in webdriver.

    # Dump demo agent into some file or Stream
    #
    # Assert on the file or Stream
