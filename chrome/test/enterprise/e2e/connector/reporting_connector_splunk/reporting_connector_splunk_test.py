# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import flags
import os
import re
import requests  # pylint: disable=import-error
from datetime import datetime

from infra import ChromeEnterpriseTestCase
from chrome_ent_test.infra.core import before_all, category, environment, test
from splunk_server import SplunkApiService


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorwithSplunkTest(ChromeEnterpriseTestCase):
  """Test the Realtime Reporting pipeline events"""

  def getSplunkCredentials(self):
    path = "gs://%s/secrets/splunkInstances.json" % self.gsbucket
    cmd = r'gsutil cat ' + path
    return self.RunCommand(self.win_config['dc'], cmd).rstrip()

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.EnableUITest(self.win_config['client'])

  @test
  def test_browser_enrolled_prod(self):
    eventFound = False
    path = "gs://%s/secrets/CELabOrg-enrollToken" % self.gsbucket
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip()
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')

    self.SetPolicy(self.win_config['dc'], r'SafeBrowsingEnabled', 1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    testStartTime = datetime.utcnow()
    # trigger malware event & get device id from browser
    dir = os.path.dirname(os.path.abspath(__file__))
    clientId = self.RunUITest(
        self.win_config['client'],
        os.path.join(dir, 'reporting_connector_ui_test.py'),
        timeout=600)
    clientId = re.search(r'DeviceId:.*$',clientId.strip()).group(0) \
      .replace('DeviceId:','')
    splunkCrendential = self.getSplunkCredentials()
    eventFound = SplunkApiService(splunkCrendential).lookupEvents(
        eventName='dangerousDownloadEvent',
        startTime=testStartTime,
        deviceId=clientId)
    self.assertTrue(eventFound)
