# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from datetime import datetime

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase
from .splunk_server import SplunkApiService


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorwithSplunkTest(ChromeEnterpriseTestCase):
  """Test the Realtime Reporting pipeline events"""

  def getSplunkCredentials(self):
    path = "gs://%s/secrets/splunkInstances.json" % self.gsbucket
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
    splunkCrendential = self.getSplunkCredentials()
    eventFound = SplunkApiService(splunkCrendential).lookupEvents(
        eventName='dangerousDownloadEvent',
        startTime=testStartTime,
        deviceId=clientId)
    self.assertTrue(eventFound)
