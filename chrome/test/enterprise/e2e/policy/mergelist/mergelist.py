# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
from absl import flags
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase

FLAGS = flags.FLAGS
flags.DEFINE_string(
    'enrollmentToken', None,
    'The enrollment token to use, it overwrites the default token')


@category("chrome_only")
@environment(file="../policy_test.asset.textpb")
class MergelistTest(ChromeEnterpriseTestCase):
  """Test the PolicyListMultipleSourceMergeList policy.
    https://cloud.google.com/docs/chrome-enterprise/policies/?policy=PolicyListMultipleSourceMergeList"""

  _CHROMIUM_URL = 'https://chromium.org'
  _GOOGLE_URL = 'https://google.com'
  _ERR_BLOCKED_BY_ADMINISTRATOR = 'is blocked'

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def enroll_in_cbcm(self):
    token = FLAGS.enrollmentToken
    if token == None:
      path = "gs://%s/secrets/mergelist_enrollmentToken" % self.gsbucket
      cmd = r'gsutil cat ' + path
      token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

  def open_page(self, url):
    args = ['--url', url, '--wait_before_page_load', '15', '--text_only']

    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunWebDriverTest(self.win_config['client'],
                                   os.path.join(dir, '../open_page.py'), args)
    return output

  @test
  def test_PolicyListMultipleSourceMergeList_enabled(self):
    self.enroll_in_cbcm()

    # Configure PolicyListMultipleSourceMergeList.
    self.SetPolicy(self.win_config['dc'],
                   r'PolicyListMultipleSourceMergeList\1', 'URLBlocklist',
                   'String')

    # Configure URLBlocklist.
    self.SetPolicy(self.win_config['dc'], r'URLBlocklist\1', self._GOOGLE_URL,
                   'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Google is blocked by the blocklist policy set at the platform level.
    output = self.open_page(self._GOOGLE_URL)
    self.assertIn(self._ERR_BLOCKED_BY_ADMINISTRATOR, output)

    # Chromium is blocked by the blocklist policy set through CBCM.
    output = self.open_page(self._CHROMIUM_URL)
    self.assertIn(self._ERR_BLOCKED_BY_ADMINISTRATOR, output)

  @test
  def test_PolicyListMultipleSourceMergeList_unset(self):
    self.enroll_in_cbcm()

    self.SetPolicy(self.win_config['dc'],
                   r'PolicyListMultipleSourceMergeList\1', 'placeholder',
                   'String')

    # Configure URLBlocklist.
    self.SetPolicy(self.win_config['dc'], r'URLBlocklist\1', self._GOOGLE_URL,
                   'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Google is blocked by the blocklist policy set at the platform level.
    output = self.open_page(self._GOOGLE_URL)
    self.assertIn(self._ERR_BLOCKED_BY_ADMINISTRATOR, output)

    # Chromium is not blocked since the CBCM value is overridden.
    output = self.open_page(self._CHROMIUM_URL)
    self.assertNotIn(self._ERR_BLOCKED_BY_ADMINISTRATOR, output)
