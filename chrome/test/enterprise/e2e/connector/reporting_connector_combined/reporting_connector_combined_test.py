# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import concurrent.futures
from datetime import datetime
import logging
from typing import List

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from .. import ChromeReportingConnectorTestCase
from .. import Verifyable
from .. import VerifyContent
from ..realtime_reporting_bce import reporting_server
from ..reporting_connector_chronicle import chronicle_api_service
from ..reporting_connector_crowdstrike import crowdstrike_humio_api_service
from ..reporting_connector_pan import pan_api_service
from ..reporting_connector_pubsub import pubsub_api_service
from ..reporting_connector_splunk import splunk_server


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorCombinedTest(ChromeReportingConnectorTestCase):
  """Test the Realtime Reporting pipeline events"""

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  def runner(self, api_service: Verifyable, deviceId: str,
             testStartTime: datetime) -> bool:
    try:
      self.TryVerifyUntilTimeout(
          verifyClass=api_service,
          content=VerifyContent(deviceId=deviceId, timestamp=testStartTime))
    except Exception:
      return False
    return True

  def _create_api_services(self) -> List[Verifyable]:
    api_services = []
    gcs_assets = {
        'secrets/ServiceAccountKey.json':
            reporting_server.RealTimeReportingServer,
        'secrets/splunkInstances.json':
            splunk_server.SplunkApiService,
        'secrets/chronicleCredentials.json':
            chronicle_api_service.ChronicleApiService,
        'secrets/pubsubCredentials.json':
            pubsub_api_service.PubsubApiService,
        'secrets/humio_user_token':
            crowdstrike_humio_api_service.CrowdStrikeHumioApiService,
        'secrets/panCredentials.json':
            pan_api_service.PanApiService,
    }
    for asset, creator in gcs_assets.items():
      api_services.append(creator(self.GetFileFromGCSBucket(asset)))
    return api_services

  @test
  def test_browser_enrolled_prod(self):
    token = self.GetCELabDefaultToken()
    self.EnrollBrowserToDomain(token)
    self.EnableSafeBrowsing()
    self.UpdatePoliciesOnClient()
    testStartTime = datetime.utcnow()
    result_vals = []

    # trigger malware event & get device id from browser
    deviceId, histogram = self.TriggerUnsafeBrowsingEvent()
    logging.info('Histogram: %s', histogram)
    self.assertIn('Enterprise.ReportingEventUploadSuccess', histogram)
    self.assertIn('count', histogram['Enterprise.ReportingEventUploadSuccess'])
    self.assertIsNotNone(
        histogram['Enterprise.ReportingEventUploadSuccess']['count'])
    self.assertIn('sum_value',
                  histogram['Enterprise.ReportingEventUploadSuccess'])
    self.assertIsNotNone(
        histogram['Enterprise.ReportingEventUploadSuccess']['sum_value'])

    with concurrent.futures.ThreadPoolExecutor() as executor:
      futures = []
      api_services = self._create_api_services()
      for api_service in api_services:
        futures.append(
            executor.submit(self.runner, api_service, deviceId, testStartTime))
      for future in concurrent.futures.as_completed(futures):
        result_vals.append(future.result())

    threshold = 3
    count = 0
    for result in result_vals:
      if result:
        count += 1
    self.assertGreaterEqual(count, threshold)