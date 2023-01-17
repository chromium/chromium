# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
import logging
import os
import re
import time

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from infra import ChromeEnterpriseTestCase

from .pan_api_service import PanApiService
from .pan_event import PanEvent


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorPanTest(ChromeEnterpriseTestCase):
  """Test the Realtime Reporting pipeline events"""

  def getCredentials(self) -> str:
    path = "gs://%s/secrets/panCredentials.json" % self.gsbucket
    cmd = r'gsutil cat ' + path
    return self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  @test
  def test_browser_enrolled_prod(self):
    eventFound = False
    path = "gs://%s/secrets/CELabOrg-enrollToken" % self.gsbucket
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')

    self.SetPolicy(self.win_config['dc'], r'SafeBrowsingEnabled', 1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    testStartTime = datetime.utcnow()
    # trigger malware event & get device id from browser
    localDir = os.path.dirname(os.path.abspath(__file__))
    commonDir = os.path.dirname(localDir)
    clientId = ''
    retryCount = 0
    # if not be able to find testsafebrowsing in logging, retry up to
    # 3 times.
    while retryCount < 3:
      retryCount += 1
      safeNet = ''
      clientId = self.RunUITest(
          self.win_config['client'],
          os.path.join(commonDir, 'common', 'realtime_reporting_ui_test.py'),
          timeout=600)
      safeNet = re.search(r'testsafebrowsing', clientId.strip())
      if safeNet:
        break
    clientId = re.search(r'DeviceId:.*$',
                         clientId.strip()).group(0).replace('DeviceId:',
                                                            '').rstrip("\\rn'")

    event_to_query = PanEvent(
        type="badNavigationEvent",
        device_id=clientId,
        reason="MALWARE",
        url="http://testsafebrowsing.appspot.com/s/malware.html",
    )
    logging.info("Event to look for: %s" % event_to_query)
    # wait until events are logged in the connector
    max_wait_time_secs = 360
    delta_secs = 10
    total_wait_time_secs = 0
    credentials = self.getCredentials()
    panService = PanApiService(credentials)
    match_found = False
    # initial wait 5 mins for events from google central server to pan
    time.sleep(300)

    while total_wait_time_secs < max_wait_time_secs:
      panService.start_xdr_query()
      panService.get_xdr_query_results()
      events = panService.get_events()
      event_string = "\n".join(str(v) for v in events)
      logging.info(f"Events logged:\n{event_string}")
      if panService.query_for_event(event_to_query):
        logging.info("Matched event found\n")
        match_found = True
        break
      panService.stop_xdr_query()
      time.sleep(delta_secs)
      total_wait_time_secs += delta_secs

    if not match_found:
      raise Exception('Timed out ' 'badNavigationEvent event not found')
