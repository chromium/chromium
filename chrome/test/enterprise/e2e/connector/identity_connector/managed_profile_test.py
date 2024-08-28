# Copyright 2024 The Chromium Authors
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
@environment(file='../single_client_test.asset.textpb')
class ManagedProfileTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.EnableUITest('client2022')
    self.InstallChrome('client2022')

  @test
  def test_icebreaker_enrollment(self):
    icebreaker_account = "demotest@croskir.onmicrosoft.com"
    icebreaker_passwd = self.GetFileFromGCSBucket('secrets/icebreakerPassword')
    dir = os.path.dirname(os.path.abspath(__file__))
    args = [
        '--%', '--account', icebreaker_account, '--password', icebreaker_passwd
    ]

    output = self.RunUITest(
        'client2022',
        os.path.join(dir, 'profile_enrollment_ui_test.py'),
        args=args)
    self.assertIn("Icebreaker work profile created", output)

    # Verify managed profile status legend
    self.assertIn('User policies', output)
    self.assertIn(icebreaker_account, output)

  @test
  def test_dasherless_enrollment(self):
    dasherless_account = "enterprisetest@croskir.onmicrosoft.com"
    dasherless_passwd = self.GetFileFromGCSBucket('secrets/dasherlessPassword')
    dir = os.path.dirname(os.path.abspath(__file__))
    args = [
        '--%', '--account', dasherless_account, '--password', dasherless_passwd
    ]

    output = self.RunUITest(
        'client2022',
        os.path.join(dir, 'profile_enrollment_ui_test.py'),
        args=args)
    self.assertIn("Dasherless work profile created", output)

    # Verify managed profile status legend
    self.assertIn('User policies', output)
    self.assertIn(dasherless_account, output)
