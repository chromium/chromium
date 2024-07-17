# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import flags
import os

from infra import ChromeEnterpriseTestCase
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test


@category("chrome_only")
@environment(file="../policy_test.asset.textpb")
class CloudManagementEnrollmentTokenTest(ChromeEnterpriseTestCase):
  """Test the CloudManagementEnrollmentToken policy:
  https://cloud.google.com/docs/chrome-enterprise/policies/?policy=CloudManagementEnrollmentToken."""

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  @test
  def test_browser_enrolled_prod(self):
    # Domain: chromepizzatest.com / OrgUnit: CBCM-enrollment
    path = "gs://%s/secrets/enrollToken" % self.gsbucket
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    local_dir = os.path.dirname(os.path.abspath(__file__))

    output = self.RunWebDriverTest(self.win_config['client'],
                                   os.path.join(local_dir, '../cbcm_enroll.py'))
    # Verify CBCM status legend
    self.assertIn('Machine policies', output)
    self.assertIn('CLIENT2022', output)
    self.assertIn(token, output)
    self.assertIn('Policy cache OK', output)
