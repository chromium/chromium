# Copyright 2023 The Chromium Authors
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


@environment(file="../policy_test.asset.textpb")
class PrecedenceTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  @test
  def test_CloudPolicyOverridesPlatformPolicyEnabled(self):
    # Enable policy CloudPolicyOverridesPlatformPolicy.
    self.SetPolicy(self.win_config['dc'], 'CloudPolicyOverridesPlatformPolicy',
                   1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    logging.info('CloudPolicyOverridesPlatformPolicy DISABLED')
    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(d, 'precedence_webdriver_test.py'))
    logging.info('Precedence order: %s', output)

    # Assert that "Cloud machine" is higher precedence than "Platform machine".
    self.assertEquals(
        "Cloud machine > Platform machine > Platform user > Cloud user\n",
        output)

  @test
  def test_CloudPolicyOverridesPlatformPolicyDisabled(self):
    # Disable policy CloudPolicyOverridesPlatformPolicy.
    self.SetPolicy(self.win_config['dc'], 'CloudPolicyOverridesPlatformPolicy',
                   0, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    logging.info('CloudPolicyOverridesPlatformPolicy DISABLED')
    output = self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(d, 'precedence_webdriver_test.py'))
    logging.info('Precedence order: %s', output)

    # Assert that the default precedence order is applied.
    self.assertEquals(
        "Platform machine > Cloud machine > Platform user > Cloud user\n",
        output)
