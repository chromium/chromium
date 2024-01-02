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
  DLP_TESTING_URL = 'https://bce-testingsite.appspot.com/'
  PRINT_BLOCK_URL = 'https://www.google.com/search?q=block'

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'], system_level=True)
    self.EnableDemoAgent(self.win_config['client'])

    # OU: beyondcorp.bigr.name -> Local Content Analysis Connector OU
    lcac_token_path = 'secrets/CELabOrg-localcontent-enrollToken'
    token = self.GetFileFromGCSBucket(lcac_token_path)
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Launch the demo agent in the background
    self.RunCommand(
        self.win_config['client'],
        r'Start-Process -FilePath "agent.exe" -WorkingDirectory ' +
        r'"C:\temp\demo_agent" -WindowStyle Hidden')

    # Run dlp command to activate agent
    self.dlp_action(self.win_config['client'], self.DLP_TESTING_URL, 'paste')

  def dlp_action(self, instance_name, url, action):
    args = ['--url', url, '--action', action]
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        instance_name,
        os.path.join(dir, 'local_content_analysis_ui_test.py'),
        args=args)
    return output

  @test
  def test_local_content_analysis_paste(self):
    # Paste block keyword
    output = self.dlp_action(self.win_config['client'], self.DLP_TESTING_URL,
                             'paste')
    self.assertIn("EVENT_RESULT_BLOCKED", output)
    self.assertIn("WEB_CONTENT_UPLOAD", output)

  @test
  def test_local_content_analysis_upload(self):
    # Upload a text file with block keyword
    output = self.dlp_action(self.win_config['client'], self.DLP_TESTING_URL,
                             'upload')
    self.assertIn("EVENT_RESULT_BLOCKED", output)
    self.assertIn("FILE_UPLOAD", output)

  @test
  def test_local_content_analysis_print(self):
    # Print a web page with block keyword inside its url
    output = self.dlp_action(self.win_config['client'], self.PRINT_BLOCK_URL,
                             'print')
    self.assertIn("EVENT_RESULT_BLOCKED", output)
    self.assertIn("PAGE_PRINT", output)
