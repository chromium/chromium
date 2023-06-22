# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
import logging
import os

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
    apiService = RealTimeReportingServer(
        self.GetFileFromGCSBucket('secrets/ServiceAccountKey.json'))
    self.TryVerifyUntilTimeout(
        verifyClass=apiService, content=VerifyContent(deviceId, testStartTime))
