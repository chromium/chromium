# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
import json
import logging
import os
import re
import time
from typing import Any

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from .. import ChromeReportingConnectorTestCase
from .. import VerifyContent
from .pan_api_service import PanApiService


def parse_to_json(source: str, pattern: str) -> Any:
  """Matches string with a regex and loads to a Json object."""
  matcher = re.search(pattern, source)
  if matcher:
    json_string = matcher.group(0)
    return json.loads(json_string)
  return None


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorPanTest(ChromeReportingConnectorTestCase):
  """Test the Realtime Reporting pipeline events"""

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

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

    credentials = self.GetFileFromGCSBucket('secrets/panCredentials.json')
    panService = PanApiService(credentials)
    # initial wait 5 mins for events from google central server to pan
    time.sleep(300)
    self.TryVerifyUntilTimeout(
        verifyClass=panService,
        content=VerifyContent(deviceId=deviceId, timestamp=testStartTime))
