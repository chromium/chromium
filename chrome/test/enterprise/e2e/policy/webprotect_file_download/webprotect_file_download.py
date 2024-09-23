# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@category("chrome_only")
@environment(file="../webprotect_test.asset.textpb")
class WebProtectFileDownloadTest(ChromeEnterpriseTestCase):
  """Test the WebProtect client behaviour.

     Here are the set of E2E test cases for testing chrome download behavior
     when webprotect is enabled. The purpose of these tests is to catch chrome
     client UI regression.

  """

  @before_all
  def setup(self):
    self.EnableUITest('webprotect-1')
    self.InstallChrome('webprotect-1')

  @test
  def test_malware_scan_download(self):
    """Get token from GCS bucket"""
    path = 'gs://%s/%s' % (self.gsbucket, 'secrets/CELabOrg-enrollToken')
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

    self.SetPolicy('win2022-dc', r'CloudManagementEnrollmentToken', token,
                   'String')
    instance_name = 'webprotect-1'
    self.RunCommand(instance_name, 'gpupdate /force')

    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(
        instance_name,
        os.path.join(local_dir, 'webprotect_file_download_webdriver.py'))

    self.assertIn('Encrypted blocked', output)
    self.assertIn('Large file blocked', output)
    self.assertIn('Unknown malware scanning', output)
