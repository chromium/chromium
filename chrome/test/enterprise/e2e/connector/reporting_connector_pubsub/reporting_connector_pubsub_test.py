# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import time
from datetime import datetime

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase
from .pubsub_api_service import PubsubApiService


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorwithPubsubTest(ChromeEnterpriseTestCase):
  """Test the Realtime Reporting pipeline events"""

  def getPubsubCredentials(self):
    path = "gs://%s/secrets/pubsubCredentials.json" % self.gsbucket
    cmd = r'gsutil cat ' + path
    return self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.EnableUITest(self.win_config['client'])

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
    clientId = self.RunUITest(
        self.win_config['client'],
        os.path.join(commonDir, 'common', 'realtime_reporting_ui_test.py'),
        timeout=600)
    clientId = re.search(r'DeviceId:.*$',
                         clientId.strip()).group(0).replace('DeviceId:',
                                                            '').rstrip("\\rn'")

    # wait until events are logged in the connector
    max_wait_time_secs = 300
    delta_secs = 10
    total_wait_time_secs = 0
    pubsubCrendential = self.getPubsubCredentials()
    pubsubService = PubsubApiService(pubsubCrendential)

    while total_wait_time_secs < max_wait_time_secs:
      pubsubService.loadEvents()
      eventFound = pubsubService.doesEventExist(
          eventName='badNavigationEvent', deviceId=clientId)
      if eventFound:
        return
      time.sleep(delta_secs)
      total_wait_time_secs += delta_secs

    raise Exception('Timed out ' 'badNavigationEvent event not found')
