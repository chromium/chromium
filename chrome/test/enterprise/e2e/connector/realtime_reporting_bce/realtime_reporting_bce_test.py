# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from datetime import datetime

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from .. import ChromeReportingConnectorTestCase
from .. import VerifyContent
from .reporting_server import RealTimeReportingServer

@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class RealTimeBCEReportingPipelineTest(ChromeReportingConnectorTestCase):
  """Test the Realtime Reporting pipeline events"""

  def getServiceAccountKey(self):
    serviceAccountKey = self.GetFileFromGCSBucket(
        'secrets/ServiceAccountKey.json')
    localDir = os.path.dirname(os.path.abspath(__file__))
    filePath = os.path.join(localDir, 'service_accountkey.json')
    with open(filePath, 'w', encoding="utf-8") as f:
      f.write(serviceAccountKey)

  @before_all
  def setup(self):
    self.InstallBrowserAndEnableUITest()

  @test
  def test_browser_enrolled_prod(self):
    token = self.GetCELabDefaultToken()
    self.EnrollBrowserToDomain(token)
    self.EnableSafeBrowsing()
    self.UpdatePoliciesOnClient()
    testStartTime = datetime.utcnow()

    # trigger malware event & get device id from browser
    deviceId, histogram = self.TriggerUnsafeBrowsingEvent()
    logging.info('histogram: %s', histogram)

    # read service account private key from gs-bucket & write into local
    self.getServiceAccountKey()
    apiService = RealTimeReportingServer()
    self.TryVerifyUntilTimeout(
        verifyClass=apiService, content=VerifyContent(deviceId, testStartTime))
