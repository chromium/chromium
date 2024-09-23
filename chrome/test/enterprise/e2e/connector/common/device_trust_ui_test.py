# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import re
import time
import traceback

from absl import app
from absl import flags
from absl import logging
from histogram.util import poll_histogram
from selenium import webdriver
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import StaleElementReferenceException
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait
import test_util
from test_util import getElementFromShadowRoot
from test_util import getElementsFromShadowRoot
from test_util import shutdown_chrome


FLAGS = flags.FLAGS

flags.DEFINE_string(
    'idp_matcher', '',
    'The idp_matcher used to match a IdP site listed at chrome://policy.')

_POLICY_URL = 'chrome://policy'
_CONNECTOR_INTERNAL_URL = 'chrome://connectors-internals'
_POLICY_CONTEXT_AWARE_ACCESS_SIGNALS_ALLOWLIST = (
    'BrowserContextAwareAccessSignalsAllowlist')
_TIMEOUT = 10


def save_screenshot(driver: webdriver.Chrome, path: str) -> None:
  original_size = driver.get_window_size()
  required_width = driver.execute_script(
      'return document.body.parentNode.scrollWidth')
  required_height = driver.execute_script(
      'return document.body.parentNode.scrollHeight')
  driver.set_window_size(required_width, required_height)
  wait_element(driver, By.TAG_NAME, 'body')
  driver.find_element(By.TAG_NAME, 'body').screenshot(path)
  driver.set_window_size(original_size['width'], original_size['height'])


def wait_element(driver, by_selector, selector, timeout=_TIMEOUT * 3):
  return WebDriverWait(driver, timeout).until(
      EC.presence_of_element_located((by_selector, selector)),
      'Could not find element with selector: "{}"'.format(selector))


def main(argv):
  logging.set_verbosity(logging.INFO)

  exclude_switches = ['disable-background-networking']
  chrome_options = webdriver.ChromeOptions()
  chrome_options.add_experimental_option('excludeSwitches', exclude_switches)
  chrome_options.add_argument('--enable-stats-collection-bindings')
  results = {}
  test_cases = ['key_creation', 'key_load']

  for test_case in test_cases:
    driver = test_util.create_chrome_webdriver(chrome_options=chrome_options)
    result = {}
    logging.info('Starting test case %s ...' % test_case)

    try:
      WebDriverWait(driver=driver, timeout=_TIMEOUT)

      # Step 1: navigate to chrome://policy app
      count = 0
      idp_urls = ''
      idp_url = ''
      device_id = ''
      found_idp_urls = False
      # Wait up to 120s till the idp_urls fields are filled.
      while count < 20:
        driver.get(_POLICY_URL)
        count += 1
        # Only click `Reload-policies` button on the key_creation because
        # a newly enrolled Chrome does not have policies cached. The button
        # triggers a policy refetch which is needed.
        # For the key_load, the policies are fetched from disk.
        if test_case == 'key_creation':
          wait_element(driver, By.CSS_SELECTOR, '#reload-policies')
          driver.find_element(By.CSS_SELECTOR, '#reload-policies').click()
        wait_element(driver, By.CSS_SELECTOR, 'policy-table')
        policy_table = driver.find_element(By.CSS_SELECTOR, 'policy-table')
        row_groups = getElementsFromShadowRoot(driver, policy_table,
                                               '.policy-data')
        try:
          for group in row_groups:
            name = getElementFromShadowRoot(driver, group, '#name').text
            if not name:
              break
            if name == _POLICY_CONTEXT_AWARE_ACCESS_SIGNALS_ALLOWLIST:
              idp_urls = getElementFromShadowRoot(
                  driver, group, 'div.policy.row > div.value').text
              if idp_urls:
                logging.info(f'idp_urls = {idp_urls}')
                found_idp_urls = True
                break
          if found_idp_urls:
            break
          time.sleep(6)
        except StaleElementReferenceException:
          logging.info('StaleElementReferenceException happened, skip rest')
      wait_element(driver, By.CSS_SELECTOR, 'status-box')
      status_box = driver.find_element(By.CSS_SELECTOR, 'status-box')
      el = getElementFromShadowRoot(driver, status_box, '.status-box-fields')
      device_id = el.find_element(By.CLASS_NAME,
                                  'machine-enrollment-device-id').text
      # idp_urls could be of '["http://abc", "http://def"]'
      # remove the excess {[, ], ", '}
      idp_urls = idp_urls.translate({ord(i): None for i in '[]\"\''})
      for url in idp_urls.split(','):
        if re.search(FLAGS.idp_matcher, url):
          idp_url = url
          break

      # Step 2: navigate to chrome://connectors-internals app
      count = 0
      dtc_policy_enabled = ''
      key_manager_initialized = ''
      spki_hash = ''
      key_trust_level = ''
      key_sync = ''
      while count < 20:
        count += 1
        driver.get(_CONNECTOR_INTERNAL_URL)
        wait_element(driver, By.CSS_SELECTOR, 'connectors-internals-app')
        connectors_internals_app = driver.find_element(
            By.CSS_SELECTOR, 'connectors-internals-app')
        connectors_tabs = getElementFromShadowRoot(driver,
                                                   connectors_internals_app,
                                                   'connectors-tabs')
        device_trust_connector = getElementFromShadowRoot(
            driver, connectors_tabs, 'device-trust-connector')
        dtc_policy_enabled = getElementFromShadowRoot(driver,
                                                      device_trust_connector,
                                                      '#enabled-string').text
        key_manager_initialized = getElementFromShadowRoot(
            driver, device_trust_connector, '#key-manager-state').text
        spki_hash = getElementFromShadowRoot(driver, device_trust_connector,
                                             '#spki-hash').text
        key_trust_level = getElementFromShadowRoot(driver,
                                                   device_trust_connector,
                                                   '#key-trust-level').text
        key_sync = getElementFromShadowRoot(driver, device_trust_connector,
                                            '#key-sync').text
        if dtc_policy_enabled.casefold() == 'true'.casefold(
        ) and key_manager_initialized.casefold() == 'true'.casefold(
        ) and '200' in key_sync:
          break
        time.sleep(6)

      result['KeyManagerInitialized'] = key_manager_initialized
      result['DTCPolicyEnabled'] = dtc_policy_enabled
      result['SpkiHash'] = spki_hash
      result['KeySync'] = key_sync
      result['KeyTrustLevel'] = key_trust_level

      # retrieve the other fields from signals
      getElementFromShadowRoot(driver, device_trust_connector,
                               '#copy-signals').click()

      ci_signals = ''
      # *[@id="device-trust-connector"]/device-trust-connector//div[3]
      # or 'div:nth-child(5)'
      # or #signals
      content = getElementFromShadowRoot(driver, device_trust_connector,
                                         '#signals').text
      # trim off 'Signals: {...} Copy Signals
      content = content.replace('Signals:', '')
      content = content.replace('Copy Signals', '')
      try:
        ci_signals = json.loads(content)
      except json.decoder.JSONDecodeError:
        logging.info('content retrieved: %s', content)
        ci_signals = content

      logging.info('signals: %s' % ci_signals)
      result['ConnectorsInternalsSignals'] = ci_signals

      # Step 3: navigate to fake IdP homepage
      logging.info(f'fake_idp:{idp_url}')
      result['FakeIdP'] = idp_url
      result['DeviceId'] = device_id
      driver.get(idp_url)
      wait_element(driver, By.XPATH, '//*[@id="content"]/div[2]/a')
      driver.find_element(By.XPATH, '//*[@id="content"]/div[2]/a').click()

      # Check client/server signals
      WebDriverWait(driver=driver, timeout=_TIMEOUT)
      try:
        wait_element(driver, By.XPATH, '//*[@id="serverSignals"]')
        server_signals = json.loads(
            driver.find_element(By.XPATH, '//*[@id="serverSignals"]').text)

        wait_element(driver, By.XPATH, '//*[@id="clientSignals"]')
        client_signals = json.loads(
            driver.find_element(By.XPATH, '//*[@id="clientSignals"]').text)
        result['ClientSignals'] = client_signals
        result['ServerSignals'] = server_signals
      except NoSuchElementException:
        logging.info('No such element found! trying to find error message')
        wait_element(driver, By.XPATH, '//*[@id="errorMessage"]/pre')
        err_msg = json.loads(
            driver.find_element(By.XPATH, '//*[@id="errorMessage"]/pre').text)
        logging.info('error: %s' % err_msg)
        result['SignalError'] = err_msg

      # Check histograms
      hg = poll_histogram(driver, [
          'Enterprise.DeviceTrust.Persistence.StoreKeyPair.Error',
          'Enterprise.DeviceTrust.Persistence.LoadKeyPair.Error',
          'Enterprise.DeviceTrust.Persistence.CreateKeyPair.Error',
          'Enterprise.DeviceTrust.RotateSigningKey.NoNonce.Status',
          'Enterprise.DeviceTrust.RotateSigningKey.NoNonce.UploadCode',
          'Enterprise.DeviceTrust.RotateSigningKey.Tries',
          'Enterprise.DeviceTrust.ManagementService.Error',
          'Enterprise.DeviceTrust.Attestation.Result',
          'Enterprise.DeviceTrust.KeyRotationCommand.Error',
          'Enterprise.DeviceTrust.KeyRotationCommand.Error.Hresult',
          'Enterprise.DeviceTrust.KeyRotationCommand.ExitCode',
      ])
      if hg:
        result['Histograms'] = hg

      results[test_case] = result
      shutdown_chrome()

    except Exception as error:
      logging.info('test_case: %s, result dict: %s' % (test_case, result))
      logging.error(error)
      traceback.print_exc()
      save_screenshot(driver, r'c:\temp\screen.png')
    finally:
      driver.quit()
  # Replace single quotes inside string to double quotes in order
  # facilitates json.loads() method call. Single quotes are not
  # JSON specification - RFC7159 compatible.
  logging.info(f'Results:{json.dumps(results)}')


if __name__ == '__main__':
  app.run(main)
