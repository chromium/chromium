# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from .. import ChromeReportingConnectorTestCase


@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorClientOnlyTest(ChromeReportingConnectorTestCase):
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

    # trigger malware event & get device id from browser
    _, histogram = self.TriggerUnsafeBrowsingEvent()
    logging.info('Histogram: %s', histogram)
    self.assertIn('Enterprise.ReportingEventUploadSuccess', histogram)
    self.assertIn('count', histogram['Enterprise.ReportingEventUploadSuccess'])
    self.assertIsNotNone(
        histogram['Enterprise.ReportingEventUploadSuccess']['count'])
    self.assertIn('sum_value',
                  histogram['Enterprise.ReportingEventUploadSuccess'])
    self.assertIsNotNone(
        histogram['Enterprise.ReportingEventUploadSuccess']['sum_value'])
