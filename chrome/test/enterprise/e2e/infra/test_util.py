# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains utility methods that can be used by python tests on Windows."""

import os
import time
from typing import NamedTuple
import win32con
import win32gui

try:
  import pywinauto
  _UI_TEST = True
except ImportError:
  _UI_TEST = False
from selenium import webdriver
from selenium.common.exceptions import StaleElementReferenceException
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait


def _window_enum_handler(hwnd, window_list):
  win_title = win32gui.GetWindowText(hwnd)
  if 'Google Chrome' in win_title or 'Chromium' in win_title:
    window_list.append(hwnd)


def _get_chrome_windows():
  """Gets the list of hwnd of Chrome windows."""
  window_list = []
  win32gui.EnumWindows(_window_enum_handler, window_list)
  return window_list


def shutdown_chrome():
  """Shutdown Chrome cleanly.

    Surprisingly there is no easy way in chromedriver to shutdown Chrome
    cleanly on Windows. So we have to use win32 API to do that: we find
    the chrome window first, then send WM_CLOSE message to it.
  """
  window_list = _get_chrome_windows()
  if not window_list:
    raise RuntimeError("Cannot find chrome windows")

  for win in window_list:
    win32gui.SendMessage(win, win32con.WM_CLOSE, 0, 0)

  # wait a little bit for chrome processes to end.
  # TODO: the right way is to wait until there are no chrome.exe processes.
  time.sleep(2)


class Policy(NamedTuple):
  value: str
  source: str
  scope: str
  messages: str


# TODO(crbug.com/399483772): Update tests to use this to get policies.
def fetch_policies(driver: webdriver.Chrome,
                   refresh: bool = True) -> dict[str, Policy]:
  driver.get('chrome://policy')
  if refresh:
    clickable = EC.element_to_be_clickable((By.ID, 'reload-policies'))
    WebDriverWait(driver, 10).until(clickable).click()
  # Wait for DOM to settle.
  time.sleep(10)

  policy_table = driver.find_element(By.CSS_SELECTOR, 'policy-table')
  rows = getElementsFromShadowRoot(driver, policy_table, '.policy-data')
  policies = {}
  for row in rows:
    name = getElementFromShadowRoot(driver, row, '#name').text.strip()
    if not name:
      continue
    if name in policies:
      raise Exception(f'duplicate policy {name!r}: {policies!r}')
    policy_attrs = {}
    for attr in ['value', 'source', 'scope', 'messages']:
      policy_attrs[attr] = getElementFromShadowRoot(
          driver, row, f'div.policy.row > div.{attr}').text
    policies[name] = Policy(**policy_attrs)
  return policies


def getElementFromShadowRoot(driver, element, selector):
  """Gets a first matched WebElement from ShadowRoot."""
  if element is None:
    return None
  else:
    return driver.execute_script(
        "return arguments[0].shadowRoot.querySelector(arguments[1])", element,
        selector)


def getElementsFromShadowRoot(driver, element, selector):
  """Gets a list of matched WebElements from ShadowRoot. """
  if element is None:
    return None
  else:
    return driver.execute_script(
        "return arguments[0].shadowRoot.querySelectorAll(arguments[1])",
        element, selector)


def create_chrome_webdriver(chrome_options=None, incognito=False, prefs=None):
  """Configures and returns a Chrome WebDriver object."

  Args:
    chrome_options: The default ChromeOptions to use.
    incognito: Whether or not to launch Chrome in incognito mode.
    prefs: Profile preferences. None for defaults.
  """
  if chrome_options == None:
    chrome_options = Options()

  if incognito:
    chrome_options.add_argument('incognito')

  if prefs != None:
    chrome_options.add_experimental_option("prefs", prefs)

  os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"

  return webdriver.Chrome(
      service=Service(
          executable_path=r"C:\temp\chromedriver.exe",
          service_args=["--verbose", r"--log-path=c:\temp\chromedriver.log"]),
      options=chrome_options)


def sign_in(driver: webdriver.Chrome, account: str, password: str):
  """Sign into the given browser as a managed user.

  Note: This function will navigate the current tab away from its initial URL.
      All other tabs will remain unaffected.
  """
  if not _UI_TEST:
    raise Exception(
        "not running inside a UI test, which is required for sign-in")
  # Start sign-in from the "New Tab Page" (NTP) so that the login page opens
  # in-place instead of in a new tab.
  driver.get("chrome://newtab")
  app = pywinauto.Application(backend="uia")
  app.connect(title_re=".*Chrome|.*Chromium")
  old_window = app.top_window()
  old_window.child_window(title="You", control_type="Button").click()
  old_window.child_window(
      title="Sign in to Chrome", control_type="Button").click()

  for (selector, value) in [("input[type=email]", account),
                            ("input[type=password]", password)]:
    condition = EC.element_to_be_clickable((By.CSS_SELECTOR, selector))
    _fill_form_with_retries(driver, condition,
                            lambda element: element.send_keys(value))
    condition = EC.element_to_be_clickable(
        (By.XPATH, '//button//*[text()="Next"]'))
    _fill_form_with_retries(driver, condition, lambda element: element.click())

  # Dismiss "Your organization will manage this profile" dialog. Merge with the
  # existing unsynced profile to avoid opening another window (one for each
  # profile).
  merge_profiles = old_window.child_window(
      title="Add existing browsing data to managed profile",
      auto_id="checkbox",
      control_type="CheckBox")
  merge_profiles.wait("exists ready")
  merge_profiles.click_input()
  continue_button = old_window.child_window(
      title="Continue", auto_id="proceed-button", control_type="Button")
  continue_button.click()


def _fill_form_with_retries(driver: webdriver.Chrome,
                            condition,
                            callback,
                            attempts: int = 3):
  # The login page dynamically adds and manipulates form input elements,
  # which:
  #   1. May not be immediately clickable or interactable
  #   2. May detach from the DOM and be replaced
  #
  # The `condition` uses polling to deal with problem (1). The retries work
  # around problem (2), since the detachment can occur racily between when
  # the condition fulfills and when the input action is sent. The page should
  # eventually settle into steady state.
  for attempt in range(attempts):
    try:
      maybe_element = WebDriverWait(driver, 10).until(condition)
      callback(maybe_element)
      return
    except StaleElementReferenceException:
      if attempt + 1 == attempts:
        raise
