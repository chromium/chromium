# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
import json
import logging
import os

from google.oauth2 import service_account
from google.auth.transport.requests import Request
import requests

from infra import ChromeEnterpriseTestCase
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test


@category("chrome_only")
@environment(file="../policy_test.asset.textpb")
class CloudReportingEnabledTest(ChromeEnterpriseTestCase):
  """Test the CloudReportingEnabled policy:
  https://cloud.google.com/docs/chrome-enterprise/policies/?policy=CloudReportingEnabled."""

  ADMIN_USER_EMAIL = 'admin@chromepizzatest.com'
  SCOPES = [
      'https://www.googleapis.com/auth/admin.directory.device.chromebrowsers'
  ]

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def GetAccessToken(self, service_account_info, admin_user_email, scopes):
    """Obtains an access token using service account key and impersonation."""

    credentials = service_account.Credentials.from_service_account_info(
        service_account_info, scopes=scopes, subject=admin_user_email)

    # Refresh the token if it's invalid
    if credentials.valid is False:
      credentials.refresh(Request())
      logging.info("Access Token refreshed.")
    return credentials.token

  def GetTakeoutUrl(self, device_id):
    url = "https://www.googleapis.com/admin/directory/v1.1beta1/customer/my_customer/devices/chromebrowsers"
    # Add arguments to url
    args = f'?projection=FULL&query={device_id}&orgUnitPath=/CBCM-enrollment'
    url += args
    return url

  @test
  def test_report_and_fetch(self):
    # Domain: chromepizzatest.com / OrgUnit: CBCM-enrollment
    # CloudReporting is enabled
    cmd = f'gsutil cat gs://{self.gsbucket}/secrets/enrollToken'
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    local_dir = os.path.dirname(os.path.abspath(__file__))
    # Run test on client which triggers enrollment, policy fetch, and a report
    output = self.RunWebDriverTest(self.win_config['client'],
                                   os.path.join(local_dir, '../cbcm_enroll.py'))

    # Get the device id of the browser
    index = output.find('DEVICE_ID=')
    self.assertTrue(index > 0)
    device_id = output[index + len('DEVICE_ID='):].split()[0]

    # Get OAuth2 access token
    key = self.GetFileFromGCSBucket('secrets/cbcmapi.json')
    serviceAccountInfo = json.loads(key)
    access_token = self.GetAccessToken(serviceAccountInfo,
                                       self.ADMIN_USER_EMAIL, self.SCOPES)
    headers = {'Authorization': f'Bearer {access_token}'}

    # Query CBCM API with virtualDeviceId
    url = self.GetTakeoutUrl(device_id)
    response = requests.get(url, headers=headers)
    logging.info('Querying CBCM API for browser data...')
    logging.info('server response = %s' % response)
    json_object = response.json()
    logging.info(json_object)

    last_registration_time = datetime.fromisoformat(
        json_object['browsers'][0]['lastRegistrationTime'])
    last_policy_fetch_time = datetime.fromisoformat(
        json_object['browsers'][0]['lastPolicyFetchTime'])
    last_status_report_time = datetime.fromisoformat(
        json_object['browsers'][0]['lastStatusReportTime'])

    self.assertTrue(int(json_object['browsers'][0]['policyCount']) > 0)
    self.assertTrue(last_policy_fetch_time > last_registration_time)
    self.assertTrue(last_status_report_time > last_registration_time)
