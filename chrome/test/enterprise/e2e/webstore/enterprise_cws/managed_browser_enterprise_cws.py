# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@category('chrome_only')
@environment(file="../policy_test.asset.textpb")
class ManagedBrowserEnterpriseWebStore(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  @test
  def test_enterprise_web_store(self):
    # Domain enrollment
    path = "gs://%s/secrets/CWStoken" % self.gsbucket
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    local_dir = os.path.dirname(os.path.abspath(__file__))

    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(local_dir, '../enterprise_cws_webdriver.py'))

    # Verify Enterprise signals
    print(output)
