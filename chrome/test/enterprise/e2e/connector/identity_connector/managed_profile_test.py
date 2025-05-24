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

AUTH_CODE_AUTH_URL = (
    r"https://chromeenterprise.google/profile-enrollment/register"
    r"?configuration="
    r"CggwNGtudzZuZxIkRTdEQkNGRTQtRUIwOC00MEZBLThENjItQjRGREU5MTE2MDZC")

IMPLICIT_AUTH_URL = (
    r"https://login.microsoftonline.com/organizations/"
    r"oauth2/v2.0/authorize?"
    r"client_id=bfb3dc57-d2b9-4b83-9a1d-7bf987e115fd&"
    r"redirect_uri=https://chromeenterprise.google/enroll/&"
    r"response_type=token%20id_token&"
    r"scope=openid+email+profile&"
    r"nonce=072f41d79a3cda30b589143eba6cd479140aa51c545f813365f839b4967d0347&"
    r"prompt=select_account")


@category('chrome_only')
@environment(file='../single_client_test.asset.textpb')
class ManagedProfileTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.EnableUITest('client2022')
    self.InstallChrome('client2022')

  @test
  def test_icebreaker_enrollment_implicit(self):
    self._test_icebreaker_enrollment(IMPLICIT_AUTH_URL)

  @test
  def test_icebreaker_enrollment_auth_code(self):
    self._test_icebreaker_enrollment(AUTH_CODE_AUTH_URL)

  @test
  def test_dasherless_enrollment_implicit(self):
    self._test_dasherless_enrollment(IMPLICIT_AUTH_URL)

  @test
  def test_dasherless_enrollment_auth_code(self):
    self._test_dasherless_enrollment(AUTH_CODE_AUTH_URL)

  def _test_icebreaker_enrollment(self, auth_url):
    icebreaker_account = "demotest@croskir.onmicrosoft.com"
    icebreaker_passwd = self.GetFileFromGCSBucket('secrets/icebreakerPassword')
    dir = os.path.dirname(os.path.abspath(__file__))
    args = [
        '--%',
        '--account',
        icebreaker_account,
        '--password',
        icebreaker_passwd,
        # We wrap the auth url in quotes because '&' are special characters
        '--auth_url',
        f"'{auth_url}'"
    ]

    output = self.RunUITest(
        'client2022',
        os.path.join(dir, 'profile_enrollment_ui_test.py'),
        args=args)
    self.assertIn("Icebreaker work profile created", output)

    # Verify managed profile status legend
    self.assertIn('User policies', output)
    self.assertIn(icebreaker_account, output)

  def _test_dasherless_enrollment(self, auth_url):
    dasherless_account = "enterprisetest@croskir.onmicrosoft.com"
    dasherless_passwd = self.GetFileFromGCSBucket('secrets/dasherlessPassword')
    dir = os.path.dirname(os.path.abspath(__file__))
    args = [
        '--%',
        '--account',
        dasherless_account,
        '--password',
        dasherless_passwd,
        # We wrap the auth url in quotes because '&' are special characters
        '--auth_url',
        f"'{auth_url}'"
    ]

    output = self.RunUITest(
        'client2022',
        os.path.join(dir, 'profile_enrollment_ui_test.py'),
        args=args)
    self.assertIn("Dasherless work profile created", output)

    # Verify managed profile status legend
    self.assertIn('User policies', output)
    self.assertIn(dasherless_account, output)
