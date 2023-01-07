# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class ChromeDriverException(Exception):
  pass
class NoSuchElement(ChromeDriverException):
  pass
class NoSuchFrame(ChromeDriverException):
  pass
class UnknownCommand(ChromeDriverException):
  pass
class StaleElementReference(ChromeDriverException):
  pass
class ElementNotVisible(ChromeDriverException):
  pass
class InvalidElementState(ChromeDriverException):
  pass
class UnknownError(ChromeDriverException):
  pass
class JavaScriptError(ChromeDriverException):
  pass
class XPathLookupError(ChromeDriverException):
  pass
class Timeout(ChromeDriverException):
  pass
class NoSuchWindow(ChromeDriverException):
  pass
class InvalidCookieDomain(ChromeDriverException):
  pass
class ScriptTimeout(ChromeDriverException):
  pass
class InvalidSelector(ChromeDriverException):
  pass
class SessionNotCreated(ChromeDriverException):
  pass
class InvalidSessionId(ChromeDriverException):
  pass
class UnexpectedAlertOpen(ChromeDriverException):
  pass
class NoSuchAlert(ChromeDriverException):
  pass
class NoSuchCookie(ChromeDriverException):
  pass
class InvalidArgument(ChromeDriverException):
  pass
class ElementNotInteractable(ChromeDriverException):
  pass
class UnsupportedOperation(ChromeDriverException):
  pass
class NoSuchShadowRoot(ChromeDriverException):
  pass
class DetachedShadowRoot(ChromeDriverException):
  pass
class WebSocketException(ChromeDriverException):
  pass
class WebSocketConnectionClosedException(WebSocketException):
  pass
class WebSocketTimeoutException(WebSocketException):
  pass