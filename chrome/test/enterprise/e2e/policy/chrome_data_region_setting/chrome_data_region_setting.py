# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
from absl import flags
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS


@environment(file="../chromedataregionsetting_test.asset.textpb")
class ChromeDataRegionSettingTest(ChromeEnterpriseTestCase):
  """Test ChromeDataRegionSetting policy.

  See:
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ChromeDataRegionSetting
  """

  @before_all
  def setup(self):
    self.InstallChrome('cbcmdrz-nopref')
    self.InstallWebDriver('cbcmdrz-nopref')
    self.InstallChrome('cbcmdrz-europe')
    self.InstallWebDriver('cbcmdrz-europe')
    self.InstallChrome("drz-user-nopref")
    self.EnableUITest("drz-user-nopref")
    self.InstallChrome("drz-user-europe")
    self.EnableUITest("drz-user-europe")

  @test
  def test_cloudMachine_ChromeDataRegionSettingNoPreference(self):
    # Domain: chromepizzatest.com / OrgUnit: CBCM-DRZ > No Preference
    # Enroll browser in OU with ChromeDataRegionSetting set to No Preference (0)
    path = "gs://%s/secrets/ChromeDataRegionSettingNoPref-enrollToken" % (
        self.gsbucket)
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()
    self.SetPolicy('win2022-dc', r'CloudManagementEnrollmentToken', token,
                   'String')

    instance_name = 'cbcmdrz-nopref'
    self.RunCommand(instance_name, 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        instance_name,
        os.path.join(d, 'chrome_data_region_setting_webdriver_test.py'))
    logging.info('output: %s', output)

    # Assert that ChromeDataRegionSetting is set to No Preference (0)
    self.assertIn('value=0', output)
    self.assertIn('source=Cloud', output)
    self.assertIn('scope=Machine', output)
    self.assertIn('status=OK', output)

  @test
  def test_cloudMachine_ChromeDataRegionSettingEurope(self):
    # Domain: chromepizzatest.com / OrgUnit: CBCM-DRZ > Europe
    # Enroll browser in OU with ChromeDataRegionSetting set to Europe (2)
    path = "gs://%s/secrets/ChromeDataRegionSettingEurope-enrollToken" % (
        self.gsbucket)
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()
    self.SetPolicy('win2022-dc', r'CloudManagementEnrollmentToken', token,
                   'String')

    instance_name = 'cbcmdrz-europe'
    self.RunCommand(instance_name, 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        instance_name,
        os.path.join(d, 'chrome_data_region_setting_webdriver_test.py'))

    # Assert that ChromeDataRegionSetting is set to Europe (2)
    self.assertIn('value=2', output)
    self.assertIn('source=Cloud', output)
    self.assertIn('scope=Machine', output)
    self.assertIn('status=OK', output)

  @test
  def test_cloudUser_ChromeDataRegionSettingNoPreference(self):
    # Domain: chromepizzatest.com
    # OrgUnit: CBCM testing > Policy Testing > Automated2
    # User belongs to an OU with ChromeDataRegionSetting set to No Preference (0)
    account = "account2@chromepizzatest.com"
    path = f"gs://{self.gsbucket}/secrets/account2-password"
    cmd = r"gsutil cat " + path
    password = self.RunCommand(self.win_config["dc"], cmd).strip().decode()

    instance_name = "drz-user-nopref"
    d = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(
        instance_name,
        os.path.join(d, "chrome_data_region_setting_webdriver_test.py"),
        args=["--account", account, "--password", password])

    # Assert that ChromeDataRegionSetting is set to No Preference (0)
    self.assertIn("value=0", output)
    self.assertIn("source=Cloud", output)
    self.assertIn("scope=Current user", output)
    self.assertIn("status=OK", output)

  @test
  def test_cloudUser_ChromeDataRegionSettingEurope(self):
    # Domain: chromepizzatest.com
    # OrgUnit: CBCM testing > Policy Testing > Automated1
    # User belongs to an OU with ChromeDataRegionSetting set to Europe (2)
    account = "account1@chromepizzatest.com"
    path = f"gs://{self.gsbucket}/secrets/account1-password"
    cmd = r"gsutil cat " + path
    password = self.RunCommand(self.win_config["dc"], cmd).strip().decode()

    instance_name = "drz-user-europe"
    d = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(
        instance_name,
        os.path.join(d, "chrome_data_region_setting_webdriver_test.py"),
        args=["--account", account, "--password", password])

    # Assert that ChromeDataRegionSetting is set to Europe (2)
    self.assertIn("value=2", output)
    self.assertIn("source=Cloud", output)
    self.assertIn("scope=Current user", output)
    self.assertIn("status=OK", output)
