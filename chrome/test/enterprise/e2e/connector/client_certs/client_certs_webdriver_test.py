# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import socket
import time
import traceback
from typing import Sequence

from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.remote.shadowroot import ShadowRoot

from test_util import (
    create_chrome_webdriver,
    fetch_policies,
    sign_in,
)

FLAGS = flags.FLAGS
flags.DEFINE_string(
    'addr', 'test1.com:443',
    'Address (<host>:<port>) of the echo server to connect to.')
flags.DEFINE_string(
    'account', None,
    'Sign into the browser as this account before refreshing policies')
flags.DEFINE_string('password', None, 'Account password')
# Write results to a file instead of stdout because `run_ui_test.py` adds its
# own logs, making the combined output not easily machine-readable.
flags.DEFINE_string('results', r'c:\temp\results.json',
                    'Path to write results to.')


# For an unknown reason, the client VM can't connect over TCP for ~5 minutes
# initially when running this test from a cold start (i.e., without
# `--nodeploy`). Use a very long default timeout to mitigate this.
#
# TODO(crbug.com/327797500): Find a permanent solution after finding the root
# cause.
def wait_for_connectivity(host: str, port: int, timeout: float = 10 * 60):
  deadline = time.monotonic() + timeout
  while time.monotonic() < deadline:
    test_socket = socket.socket()
    test_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    test_socket.settimeout(60)
    try:
      test_socket.connect((host, port))
      return
    except OSError:
      traceback.print_exc()
      time.sleep(5)
    finally:
      test_socket.close()
  # A firewall misconfiguration is likely (either Windows's native firewall
  # or Google Cloud's).
  raise TimeoutError(
      f'{host}:{port} not accepting connections after {timeout:.3f}s')


def main(argv):
  host, _, port = FLAGS.addr.rpartition(':')
  port = int(port)
  wait_for_connectivity(host, port)

  options = webdriver.ChromeOptions()
  # Expose Chrome UI elements to `pywinauto`.
  options.add_argument('--force-renderer-accessibility')
  # Bypass a basic CAPTCHA by not identifying as a WebDriver-controlled browser
  # (i.e., sets `navigator.webdriver` to false).
  options.add_argument('--disable-blink-features=AutomationControlled')
  # Override chromedriver's default of disabling sync/sign-in.
  options.add_experimental_option('excludeSwitches', ['disable-sync'])

  driver = create_chrome_webdriver(chrome_options=options)
  # Wait up to 10s for finding elements.
  driver.implicitly_wait(10)

  try:
    if FLAGS.account and FLAGS.password:
      sign_in(driver, FLAGS.account, FLAGS.password)

    policies_by_name, fingerprints = {}, {}
    for name, policy in fetch_policies(driver).items():
      policies_by_name[name] = {
          'value': policy.value,
          'source': policy.source,
          'scope': policy.scope,
      }

    # To aid diagnosis, try to get fingerprints from all sources, even if one of
    # the retrievals fails.
    for key, get_fingerprint in [
        ('connectors', get_fingerprint_from_connector_internals),
        ('cert-manager', get_fingerprint_from_cert_manager),
        ('server', lambda driver: get_cert_sent_to_server(driver, host, port)),
    ]:
      try:
        fingerprints[key] = get_fingerprint(driver)
      except Exception:
        traceback.print_exc()

    results = {'policies': policies_by_name, 'fingerprints': fingerprints}
    with open(FLAGS.results, 'w+') as results_file:
      json.dump(results, results_file)
  finally:
    driver.quit()


def get_fingerprint_from_connector_internals(driver: webdriver.Chrome) -> str:
  driver.get('chrome://connectors-internals/#managed-client-certificate')
  root = descend_shadow_roots(driver, [
      'connectors-internals-app',
      'connectors-tabs',
      'managed-client-certificate',
  ])
  for div in root.find_elements(By.CSS_SELECTOR,
                                '#managed-identities > div > div'):
    if 'SHA-256 Fingerprint' in div.text:
      return div.find_element(By.CSS_SELECTOR, 'span').text.strip()
  raise Exception('Fingerprint not found in chrome://connectors-internals')


def get_fingerprint_from_cert_manager(driver: webdriver.Chrome) -> str:
  driver.get('chrome://certificate-manager/clientcerts')
  root = descend_shadow_roots(driver, [
      'certificate-manager-v2',
      '#provisionedClientCerts',
      'certificate-entry-v2',
      '#certhash',
  ])
  input_elem = root.find_element(By.ID, 'input')
  return input_elem.get_property('value').strip()


def get_cert_sent_to_server(driver: webdriver.Chrome, host: str,
                            port: int) -> str:
  driver.get(f'https://{host}:{port}')
  return driver.find_element(By.TAG_NAME, 'body').text.strip()


def descend_shadow_roots(
    driver: webdriver.Chrome,
    shadow_host_selectors: Sequence[str]) -> webdriver.Chrome | ShadowRoot:
  root = driver
  for selector in shadow_host_selectors:
    root = root.find_element(By.CSS_SELECTOR, selector).shadow_root
  return root


if __name__ == '__main__':
  app.run(main)
