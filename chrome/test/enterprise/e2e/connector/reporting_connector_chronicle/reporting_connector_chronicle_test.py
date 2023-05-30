# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from datetime import datetime
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from .. import ChromeReportingConnectorTestCase
from .. import VerifyContent
from .chronicle_api_service import ChronicleApiService

@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorwithChronicleTest(ChromeReportingConnectorTestCase):
  """Test the Realtime Reporting pipeline events"""

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
    logging.info('Histogram: %s', histogram)

    # wait until events are logged in the connector
    crendentials = self.GetFileFromGCSBucket(
        'secrets/chronicleCredentials.json')
    chronicleService = ChronicleApiService(crendentials)
    self.TryVerifyUntilTimeout(
        verifyClass=chronicleService,
        content=VerifyContent(deviceId=deviceId, timestamp=testStartTime))
