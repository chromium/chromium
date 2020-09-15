# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class ExtensionInstallAllowlistTest(ChromeEnterpriseTestCase):
  """Test the ExtensionInstallBlocklist policy.
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ExtensionInstallAllowlist"""

  @before_all
  def setup(self):
    self.InstallChrome('client2019')
    self.EnableUITest('client2019')
    self.InstallWebDriver('client2019')

  def installExtension(self, url):
    args = ['--url', url]

    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunUITest(
        'client2019', os.path.join(dir, '../install_extension.py'), args=args)
    return output

  @test
  def test_ExtensionAllowlist_hangout(self):
    extension = 'nckgahadagoaajjgafhacjanaoiihapd'
    self.SetPolicy('win2019-dc', r'ExtensionInstallBlocklist\1', '*', 'String')
    self.SetPolicy('win2019-dc', r'ExtensionInstallAllowlist\1', extension,
                   'String')
    self.RunCommand('client2019', 'gpupdate /force')
    logging.info('Allowlist extension install for ' + extension +
                 ' while disabling others')

    test_url = 'https://chrome.google.com/webstore/detail/google-hangouts/nckgahadagoaajjgafhacjanaoiihapd'
    output = self.installExtension(test_url)
    self.assertIn('Not blocked', output)

    negative_test_url = 'https://chrome.google.com/webstore/detail/grammarly-for-chrome/kbfnbcaeplbcioakkpcpgfkobkghlhen'
    output = self.installExtension(negative_test_url)
    self.assertIn('blocked', output)
