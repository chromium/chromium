# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@environment(file="../genai_test.asset.textpb")
class GeminiSettingsTest(ChromeEnterpriseTestCase):
  """Test GeminiSettings policy.

  See:
     https://cloud.google.com/docs/chrome-enterprise/policies/?policy=GeminiSettings
  """

  @before_all
  def setup(self):
    self.InstallChrome('genai-client')
    self.InstallWebDriver('genai-client')
    self.InstallChrome('genai-user')
    self.EnableUITest('genai-user')

  @test
  def test_GeminiSettingsUnset_GenAiDefaultSettingsAllowedWithLogging(self):
    # Domain: chromepizzatest.com
    # OrgUnit: Policy Settings > Automated0
    # User belongs to an OU with GenAiDefaultSettings set to allowed with
    # logging.
    account = 'account0@chromepizzatest.com'
    path = f"gs://{self.gsbucket}/secrets/account0-password"
    cmd = r"gsutil cat " + path
    password = self.RunCommand(self.win_config["dc"], cmd).strip().decode()
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunUITest(
        'genai-user',
        os.path.join(d, 'gemini_settings_webdriver_test.py'),
        args=['--account', account, '--password', password])

    # Assert that the pref is managed and set to 0.
    raw_results = self.RunCommand('genai-user',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['managed'], results['metadata'])
    self.assertEqual(0, results['value'])

  @test
  def test_GeminiSettingsUnset_GenAiDefaultSettingsAllowedWithoutLogging(self):
    # Domain: chromepizzatest.com
    # OrgUnit: Policy Settings > Automated1
    # User belongs to an OU with GenAiDefaultSettings set to allowed without
    # logging.
    account = 'account1@chromepizzatest.com'
    path = f"gs://{self.gsbucket}/secrets/account1-password"
    cmd = r"gsutil cat " + path
    password = self.RunCommand(self.win_config["dc"], cmd).strip().decode()
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunUITest(
        'genai-user',
        os.path.join(d, 'gemini_settings_webdriver_test.py'),
        args=['--account', account, '--password', password])

    # Assert that the pref is managed and set to 0.
    raw_results = self.RunCommand('genai-user',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['managed'], results['metadata'])
    self.assertEqual(0, results['value'])

  @test
  def test_GeminiSettingsUnset_GenAiDefaultSettingsDoNotAllow(self):
    # Domain: chromepizzatest.com
    # OrgUnit: Policy Settings > Automated2
    # User belongs to an OU with GenAiDefaultSettings set to do not allow.
    account = 'account2@chromepizzatest.com'
    path = f"gs://{self.gsbucket}/secrets/account2-password"
    cmd = r"gsutil cat " + path
    password = self.RunCommand(self.win_config["dc"], cmd).strip().decode()
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunUITest(
        'genai-user',
        os.path.join(d, 'gemini_settings_webdriver_test.py'),
        args=['--account', account, '--password', password])

    # Assert that the pref is managed and set to 1.
    raw_results = self.RunCommand('genai-user',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['managed'], results['metadata'])
    self.assertEqual(1, results['value'])

  @test
  def test_GeminiSettingsUnset_GenAiDefaultSettingsUnset(self):
    # Domain: chromepizzatest.com
    # OrgUnit: Policy Settings > Automated3
    # User belongs to an OU with GenAiDefaultSettings unset.
    account = 'account3@chromepizzatest.com'
    path = f"gs://{self.gsbucket}/secrets/account3-password"
    cmd = r"gsutil cat " + path
    password = self.RunCommand(self.win_config["dc"], cmd).strip().decode()
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunUITest(
        'genai-user',
        os.path.join(d, 'gemini_settings_webdriver_test.py'),
        args=['--account', account, '--password', password])

    # Assert that the pref is managed and set to 0.
    raw_results = self.RunCommand('genai-user',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['managed'], results['metadata'])
    self.assertEqual(0, results['value'])

  @test
  def test_GeminiSettingsUnset(self):
    # Ensure GeminiSettings is unset.
    self.SetPolicy(self.win_config["dc"], 'GeminiSettings', 0, 'DWORD')
    self.RemovePolicy(self.win_config["dc"], 'GeminiSettings')
    self.RunCommand('genai-client', 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunWebDriverTest('genai-client',
                          os.path.join(d, 'gemini_settings_webdriver_test.py'))

    # Assert that the pref is unmanaged and set to 0.
    raw_results = self.RunCommand('genai-client',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['default', 'user_modifiable', 'extension_modifiable'],
                     results['metadata'])
    self.assertEqual(0, results['value'])

  @test
  def test_GeminiSettingsAllowed(self):
    # Set GeminiSettings to allowed (0).
    self.SetPolicy(self.win_config["dc"], 'GeminiSettings', 0, 'DWORD')
    self.RunCommand('genai-client', 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunWebDriverTest('genai-client',
                          os.path.join(d, 'gemini_settings_webdriver_test.py'))

    # Assert that the pref is managed and set to 0.
    raw_results = self.RunCommand('genai-client',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['managed'], results['metadata'])
    self.assertEqual(0, results['value'])

  @test
  def test_GeminiSettingsDisabled(self):
    # Set GeminiSettings to disabled (1).
    self.SetPolicy(self.win_config["dc"], 'GeminiSettings', 1, 'DWORD')
    self.RunCommand('genai-client', 'gpupdate /force')
    d = os.path.dirname(os.path.abspath(__file__))
    self.RunWebDriverTest('genai-client',
                          os.path.join(d, 'gemini_settings_webdriver_test.py'))

    # Assert that the pref is managed and set to 1.
    raw_results = self.RunCommand('genai-client',
                                  r'Get-Content c:\temp\results.json')
    results = json.loads(raw_results)
    self.assertEqual(['managed'], results['metadata'])
    self.assertEqual(1, results['value'])
