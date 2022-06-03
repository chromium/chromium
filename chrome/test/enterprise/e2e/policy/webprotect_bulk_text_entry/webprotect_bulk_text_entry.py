# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import category, environment, before_all, test
from infra import ChromeEnterpriseTestCase


@category("chrome_only")
@environment(file="../webprotect_test.asset.textpb")
class WebProtectBulkTextEntryTest(ChromeEnterpriseTestCase):
  """Test the WebProtect client behaviour of web content upload.

     The purpose of these tests is to catch chrome
     client UI regression when the user pastes text into the web.

  """
  blocked_text = "This data has sensitive or dangerous content. " \
               "Remove this content and try again."
  timeout = 300

  @before_all
  def setup(self):
    self.InstallChrome('webprotect-1')
    self.EnableUITest('webprotect-1')
    self.InstallChrome('webprotect-2')
    self.EnableUITest('webprotect-2')

  @test
  def test_paste_from_clipboard(self):
    # Enroll to a OU where SSN paste is blocked
    self.SetPolicy('win2016-dc', r'CloudManagementEnrollmentToken',
                   '3dd4fde1-914f-4cad-83c0-c1470954066d', 'String')
    instance_name = 'webprotect-1'
    self.RunCommand(instance_name, 'gpupdate /force')
    text = "NormalText"

    local_dir = os.path.dirname(os.path.abspath(__file__))
    args = ['--text', text]
    output = self.RunUITest(
        instance_name,
        os.path.join(local_dir, 'webprotect_bulk_text_entry_webdriver.py'),
        self.timeout, args)

    self.assertNotIn(self.blocked_text, output)
    self.assertIn("Paste is done", output)

  @test
  def test_paste_SSN_from_clipboard(self):
    # Enroll to a OU where SSN paste is blocked
    self.SetPolicy('win2016-dc', r'CloudManagementEnrollmentToken',
                   '3dd4fde1-914f-4cad-83c0-c1470954066d', 'String')
    instance_name = 'webprotect-2'
    self.RunCommand(instance_name, 'gpupdate /force')
    text = "314-20-9301"

    local_dir = os.path.dirname(os.path.abspath(__file__))
    args = ['--text', text]
    output = self.RunUITest(
        instance_name,
        os.path.join(local_dir, 'webprotect_bulk_text_entry_webdriver.py'),
        self.timeout, args)

    self.assertIn(self.blocked_text, output)
    self.assertIn("Paste is blocked", output)
