# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import flags

import logging
import os
import requests
import re
import time
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from connector import ChromeReportingConnectorTestCase


@category("chrome_only")
@environment(file="../policy_test.asset.textpb")
class ReportCbcmEvents(ChromeReportingConnectorTestCase):
  """
  This test verifies that chrome browsers that are commercially managed
  (CBCM browsers) can send events to the ChromeOS encrypted reporting server.
  """

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def RunTest(self):
    """
    Runs cbcm_enroll.py which sends heartbeat events
    to the reporting server and returns the machine/device id.
    """
    local_dir = os.path.dirname(os.path.abspath(__file__))
    return self.RunWebDriverTest(
        self.win_config['client'],
        os.path.join(local_dir, 'report_cbcm_events_webdriver.py'))

  def IsEventValid(self, event, test_start_time, device_id):
    timestamp_in_microseconds = int(
        event['apiEvent']['reportingRecordEvent']['timestampUs'])
    event_occurred_after_test_started = timestamp_in_microseconds > test_start_time
    event_occurred_on_test_device = event['clientId'] == device_id
    return event_occurred_after_test_started and event_occurred_on_test_device

  def VerifyHeartbeatEvents(self, test_start_time_in_microseconds, url,
                            device_id):
    r = requests.get(url)
    logging.info('Querying reporting server for events...')
    logging.info('server response = %s' % r)
    json_object = r.json()

    if not 'event' in json_object:
      return False

    event_count = 0
    for event in json_object['event']:
      if self.IsEventValid(event, test_start_time_in_microseconds, device_id):
        event_count += 1

    logging.info('Found %d CBCM heartbeat events' % event_count)
    return event_count > 0

  def GetReportingUrl(self, device_id, customer_id, api_key):
    url = "https://chromereporting-pa.googleapis.com/v1/test/events"
    # Add arguments to url
    args = "?key=%s&obfuscatedCustomerId=%s&deviceId=%s&destination=HEARTBEAT_EVENTS" % (
        api_key, customer_id, device_id)
    url += args
    return url

  @test
  def test_ReportCbcmEvent(self):
    test_start_time_in_microseconds = round(time.time() * 1000000)

    # Enroll browser to managedchrome.com domain
    managed_chrome_enrollment_token = self.GetManagedChromeDomainEnrollmentToken(
    )
    self.EnrollBrowserToDomain(managed_chrome_enrollment_token)

    # Enable cloud reporting
    self.SetPolicy(self.win_config['dc'], 'CloudReportingEnabled', 1, 'DWORD')
    self.UpdatePoliciesOnClient()

    # Run the test and capture output
    output = self.RunTest()

    # Get the machine/device id.
    capture_device_id_regex = r'DEVICE_ID=.*[a-zA-Z0-9-]+'
    match = re.search(capture_device_id_regex, output)
    self.assertTrue(match)
    device_id = match.group()[len('DEVICE_ID='):]

    # Customer id for managedchrome.com
    customer_id = self.GetManagedChromeCustomerId()
    api_key = self.GetEncryptedReportingAPIKey()
    url = self.GetReportingUrl(device_id, customer_id, api_key)

    self.assertTrue(
        self.VerifyHeartbeatEvents(test_start_time_in_microseconds, url,
                                   device_id))
