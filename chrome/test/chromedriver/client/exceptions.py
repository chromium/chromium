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
# NOTE: This exception is outdated in W3C standard but it might be thrown in the
# legacy mode.
class ElementNotVisible(ChromeDriverException):
  pass
class InvalidElementState(ChromeDriverException):
  pass
class UnknownError(ChromeDriverException):
  pass
class JavaScriptError(ChromeDriverException):
  pass
# NOTE: This exception is outdated in W3C standard but it might be thrown in the
# legacy mode.
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
class NoSuchHandle(ChromeDriverException):
  pass
class NoSuchIntercept(ChromeDriverException):
  pass
class NoSuchNode(ChromeDriverException):
  pass
class NoSuchRequest(ChromeDriverException):
  pass
class NoSuchScript(ChromeDriverException):
  pass
class UnableToCloseBrowser(ChromeDriverException):
  pass
class WebSocketException(ChromeDriverException):
  pass
class WebSocketConnectionClosedException(WebSocketException):
  pass
class WebSocketTimeoutException(WebSocketException):
  pass

EXCEPTION_MAP = {
  'invalid session id' : InvalidSessionId,
  'no such element': NoSuchElement,
  'no such frame': NoSuchFrame,
  'unknown command': UnknownCommand,
  'stale element reference': StaleElementReference,
  'invalid element state': InvalidElementState,
  'unknown error': UnknownError,
  'javascript error': JavaScriptError,
  'timeout': Timeout,
  'no such window': NoSuchWindow,
  'invalid cookie domain': InvalidCookieDomain,
  'unexpected alert open': UnexpectedAlertOpen,
  'no such alert': NoSuchAlert,
  'script timeout': ScriptTimeout,
  'invalid selector': InvalidSelector,
  'session not created': SessionNotCreated,
  'no such cookie': NoSuchCookie,
  'invalid argument': InvalidArgument,
  'element not interactable': ElementNotInteractable,
  'unsupported operation': UnsupportedOperation,
  'no such shadow root': NoSuchShadowRoot,
  'detached shadow root': DetachedShadowRoot,
  'no such handle': NoSuchHandle,
  'no such intercept': NoSuchIntercept,
  'no such node': NoSuchNode,
  'no such request': NoSuchRequest,
  'no such script': NoSuchScript,
  'unable to close browser': UnableToCloseBrowser,
}
