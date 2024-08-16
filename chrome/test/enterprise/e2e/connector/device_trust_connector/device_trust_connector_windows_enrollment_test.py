# Copyright 2023 The Chromium Authors
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

from infra import ChromeEnterpriseTestCase


def parse_to_json(source: str, pattern: str) -> Any:
  """Matches string with a regex and loads to a Json object."""
  matcher = re.search(pattern, source)
  if matcher:
    json_string = matcher.group(0)
    return json.loads(json_string)
  return None


@category('chrome_only')
@environment(file='../connector_test.asset.textpb')
class DeviceTrustConnectorWindowsEnrollmentTest(ChromeEnterpriseTestCase):

  @before_all
  def setup(self):
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'], system_level=True)
    self.InstallGoogleUpdater(self.win_config['client'])
    self.WakeGoogleUpdater(self.win_config['client'])
    self.AddFirewallExclusion(self.win_config['client'])

  @test
  def test_device_trust_enrollment(self):
    # To match for the right IdP site when there are multiple present
    idp_matcher = '^[htps]+[:/]+staging-.*'
    eventFound = False
    path = 'gs://%s/secrets/CELabOrg-devicetrust-enrollToken' % self.gsbucket
    cmd = r'gsutil cat ' + path
    token = self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()
    # Enable two Policies
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')

    self.SetPolicy(self.win_config['dc'], r'SafeBrowsingEnabled', 1, 'DWORD')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

    # Schedule to run device_trust_ui_test on GCP VM machines
    commonDir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    self.EnableHistogramSupport(self.win_config['client'], commonDir)

    # Run the UI test once to ensure enrollment
    self.RunUITest(
        self.win_config['client'],
        os.path.join(commonDir, 'common', 'device_trust_ui_test.py'),
        timeout=600,
        args=[
            '--idp_matcher',
            idp_matcher,
            '--alsologtostderr',
        ])

    # Trigger Google Updater via Task Scheduler
    self.RunGoogleUpdaterTaskSchedulerCommand(self.win_config['client'],
                                              'Start-ScheduledTask')
    self.WaitForUpdateCheck(self.win_config['client'])

    output = self.RunUITest(
        self.win_config['client'],
        os.path.join(commonDir, 'common', 'device_trust_ui_test.py'),
        timeout=600,
        args=[
            '--idp_matcher',
            idp_matcher,
            '--alsologtostderr',
        ])

    # Assert on the information retrieved from output
    results = parse_to_json(output, '(?<=Results:).*')
    self.assertIsNotNone(results)
    keys = ['key_creation', 'key_load']
    for key in keys:
      logging.info('Test case: %s' % key)
      result = results[key]
      self.assertEqual(result['DTCPolicyEnabled'], 'true')
      self.assertEqual(result['KeyManagerInitialized'], 'true')
      self.assertTrue('200' in result['KeySync'])
      logging.info('key_sync: %s' % result['KeySync'])
      self.assertFalse('UNKNOWN' in result['KeyTrustLevel'])
      logging.info('key_trust_level: %s' % result['KeyTrustLevel'])
      self.assertIsNotNone(result['SpkiHash'])
      logging.info('device_hash: %s' % result['SpkiHash'])
      self.assertIsNotNone(result['FakeIdP'])
      self.assertIsNotNone(result['Histograms'])
      client = result['ClientSignals']
      server = result['ServerSignals']
      self.assertEqual(client['deviceEnrollmentDomain'], 'beyondcorp.bigr.name')
      self.assertEqual(client['safeBrowsingProtectionLevel'], 'STANDARD')
      self.assertEqual(client['trigger'], 'TRIGGER_BROWSER_NAVIGATION')
      self.assertEqual(server['keyTrustLevel'], 'CHROME_BROWSER_HW_KEY')
      self.assertIsNotNone(server['devicePermanentId'])

    self.RemoveDeviceTrustKey(self.win_config['client'])
