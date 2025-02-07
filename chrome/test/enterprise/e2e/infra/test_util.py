# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains utility methods that can be used by python tests on Windows."""

import os
import time
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

  Note: This function will return with the new signed-in window selected.
      Existing tabs belonging to the previous profile will be closed.
  """
  if not _UI_TEST:
    raise Exception(
        "not running inside a UI test, which is required for sign-in")
  original_tabs = set(driver.window_handles)
  app = pywinauto.Application(backend="uia")
  app.connect(title_re=".*Chrome|.*Chromium")
  old_window = app.top_window()
  old_window.child_window(title="You", control_type="Button").click()
  old_window.child_window(
      title="Sign in to Chrome", control_type="Button").click()

  # Clicking "Sign in to Chrome" opens a new tab with the login page. Although
  # the login page has focus, `chromedriver` doesn't consider it selected
  # until we send the command to do so.
  wait = WebDriverWait(driver, 10)
  wait.until(EC.new_window_is_opened(original_tabs))
  (login_tab,) = set(driver.window_handles) - original_tabs
  driver.switch_to.window(login_tab)

  for (selector, value) in [("input[type=email]", account),
                            ("input[type=password]", password)]:
    condition = EC.element_to_be_clickable((By.CSS_SELECTOR, selector))
    _fill_form_with_retries(driver, condition,
                            lambda element: element.send_keys(value))
    condition = EC.element_to_be_clickable(
        (By.XPATH, '//button//*[text()="Next"]'))
    _fill_form_with_retries(driver, condition, lambda element: element.click())

  # Dismiss "Your organization will manage this profile" dialog.
  continue_button = old_window.child_window(
      title="Continue", auto_id="proceed-button", control_type="Button")
  continue_button.wait("exists ready")
  continue_button.click()
  # There should be two OS windows open at this point:
  # 1. Original window: The original tabs, plus the tab used to sign in.
  # 2. Signed-in window: A new tab, and a "Sync is disabled by your
  #    administrator" dialog that counts as a WebDriver browsing context.
  #
  # Close OS window (1), but only after (2) fully appears. Otherwise,
  # `chromedriver` will destroy the entire session too early.
  wait.until(EC.number_of_windows_to_be(len(original_tabs) + 3))
  old_window.close()
  wait.until(EC.number_of_windows_to_be(2))
  # Connect to the new window and dismiss the message about sync.
  app.connect(title_re=".*Chrome|.*Chromium")
  app.top_window().child_window(
      title="Continue", auto_id="confirmButton", control_type="Button").click()

  # `chromedriver` still points to the closed window, so switch to the new
  # signed-in window.
  wait.until(EC.number_of_windows_to_be(1))
  driver.switch_to.window(driver.window_handles[0])


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
