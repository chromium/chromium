#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Re-runs the ChromeDriver's client-side commands, given a log file.

Takes a ChromeDriver log file that was created with the --replayable=true
command-line flag for the ChromeDriver binary (or with the same flag for
the run_py_tests.py).

To replay a log file, just run this script with the log file specified
in the --input-log-path flag. Alternatively, construct a CommandSequence
instance and iterate over it to access the logged commands one-by-one.
Notice that for the iteration approach, you must call
CommandSequence.ingestRealResponse with each response.

Implementation:
The CommandSequence class is the core of the implementation here. At a
basic level, it opens the given log file, looks for the next command and
response pair, and returns them (along with their parameters/payload) on
NextCommand, next, or __iter__.

To get effective replay, there are a few deviations from simply verbatim
repeating the logged commands and parameters:
  1. Session, window, and element IDs in the log are identified with the
     corresponding ID in the new session and substituted in each command
     returned.
  2. When a response is an error, we need to infer other parts of the
     original response that would have been returned along with the
     error.
  3. If GetSessions is called while there are multiple sessions open,
     the log will show more calls than actually occurred (one per open
     session, even if it was only called once), so we absorb all of
     these calls back into one.
"""
import collections
import json
import optparse
import os
import re
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
_CLIENT_DIR = os.path.join(_PARENT_DIR, "client")
_SERVER_DIR = os.path.join(_PARENT_DIR, "server")

# pylint: disable=g-import-not-at-top
sys.path.insert(1, _CLIENT_DIR)
import command_executor
sys.path.remove(_CLIENT_DIR)

sys.path.insert(1, _SERVER_DIR)
import server
sys.path.remove(_SERVER_DIR)

sys.path.insert(1, _PARENT_DIR)
import util
sys.path.remove(_PARENT_DIR)
# pylint: enable=g-import-not-at-top


class Method(object):
  GET = "GET"
  POST = "POST"
  DELETE = "DELETE"

# TODO(crbug/chromedriver/2511) there should be a single source of truth for
# this data throughout chromedriver code (see e.g. http_handler.cc)
_COMMANDS = {
    "AcceptAlert": (Method.POST, "/session/:sessionId/alert/accept"),
    "AddCookie": (Method.POST, "/session/:sessionId/cookie"),
    "ClearElement": (Method.POST, "/session/:sessionId/element/:id/clear"),
    "ClearLocalStorage": (Method.DELETE, "/session/:sessionId/local_storage"),
    "ClearSessionStorage":
    (Method.DELETE, "/session/:sessionId/session_storage"),
    "Click": (Method.POST, "/session/:sessionId/click"),
    "ClickElement": (Method.POST, "/session/:sessionId/element/:id/click"),
    "CloseWindow": (Method.DELETE, "/session/:sessionId/window"),
    "DeleteActions": (Method.DELETE, "/session/:sessionId/actions"),
    "DeleteAllCookies": (Method.DELETE, "/session/:sessionId/cookie"),
    "DeleteCookie": (Method.DELETE, "/session/:sessionId/cookie/:name"),
    "DeleteNetworkConditions":
    (Method.DELETE, "/session/:sessionId/chromium/network_conditions"),
    "DeleteScreenOrientation":
    (Method.DELETE, "/session/:sessionId/orientation"),
    "DismissAlert": (Method.POST, "/session/:sessionId/dismiss_alert"),
    "DoubleClick": (Method.POST, "/session/:sessionId/doubleclick"),
    "ElementScreenshot":
    (Method.GET, "/session/:sessionId/element/:id/screenshot"),
    "ExecuteAsyncScript": (Method.POST, "/session/:sessionId/execute_async"),
    "ExecuteCDP": (Method.POST, "/session/:sessionId/goog/cdp/execute"),
    "ExecuteScript": (Method.POST, "/session/:sessionId/execute/sync"),
    "FindChildElement":
    (Method.POST, "/session/:sessionId/element/:id/element"),
    "FindChildElements":
    (Method.POST, "/session/:sessionId/element/:id/elements"),
    "FindElement": (Method.POST, "/session/:sessionId/element"),
    "FindElements": (Method.POST, "/session/:sessionId/elements"),
    "Freeze": (Method.POST, "/session/:sessionId/goog/page/freeze"),
    "FullscreenWindow": (Method.POST, "/session/:sessionId/window/fullscreen"),
    "GetActiveElement": (Method.POST, "/session/:sessionId/element/active"),
    "GetAlertMessage": (Method.GET, "/session/:sessionId/alert_text"),
    "GetCookies": (Method.GET, "/session/:sessionId/cookie"),
    "GetElementAttribute":
    (Method.GET, "/session/:sessionId/element/:id/attribute/:name"),
    "GetElementProperty":
    (Method.GET, "/session/:sessionId/element/:id/property/:name"),
    "GetElementCSSProperty":
    (Method.GET, "/session/:sessionId/element/:id/css/:propertyName"),
    "GetElementLocation":
    (Method.GET, "/session/:sessionId/element/:id/location"),
    "GetElementLocationInView":
    (Method.GET, "/session/:sessionId/element/:id/location_in_view"),
    "GetElementRect": (Method.GET, "/session/:sessionId/element/:id/rect"),
    "GetElementSize": (Method.GET, "/session/:sessionId/element/:id/size"),
    "GetElementTagName": (Method.GET, "/session/:sessionId/element/:id/name"),
    "GetElementText": (Method.GET, "/session/:sessionId/element/:id/text"),
    "GetElementValue": (Method.GET, "/session/:sessionId/element/:id/value"),
    "GetGeolocation": (Method.GET, "/session/:sessionId/location"),
    "GetLocalStorageItem":
    (Method.GET, "/session/:sessionId/local_storage/key/:key"),
    "GetLocalStorageKeys":
    (Method.GET, "/session/:sessionId/local_storage"),
    "GetLocalStorageSize":
    (Method.GET, "/session/:sessionId/local_storage/size"),
    "GetLog": (Method.POST, "/session/:sessionId/log"),
    "GetLogTypes": (Method.GET, "/session/:sessionId/log/types"),
    "GetNamedCookie": (Method.GET, "/session/:sessionId/cookie/:name"),
    "GetNetworkConditions":
    (Method.GET, "/session/:sessionId/chromium/network_conditions"),
    "GetNetworkConnection":
    (Method.GET, "/session/:sessionId/network_connection"),
    "GetScreenOrientation": (Method.GET, "/session/:sessionId/orientation"),
    "GetSessionCapabilities": (Method.GET, "/session/:sessionId"),
    "GetSessionStorageItem":
    (Method.GET, "/session/:sessionId/session_storage/key/:key"),
    "GetSessionStorageKeys":
    (Method.GET, "/session/:sessionId/session_storage"),
    "GetSessionStorageSize":
    (Method.GET, "/session/:sessionId/session_storage/size"),
    "GetSessions": (Method.GET, "/sessions"),
    "GetSource": (Method.GET, "/session/:sessionId/source"),
    "GetStatus": (Method.GET, "status"),
    "GetTimeouts": (Method.GET, "/session/:sessionId/timeouts"),
    "GetTitle": (Method.GET, "/session/:sessionId/title"),
    "GetUrl": (Method.GET, "/session/:sessionId/url"),
    "GetWindow": (Method.GET, "/session/:sessionId/window_handle"),
    "GetWindowPosition":
    (Method.GET, "/session/:sessionId/window/:windowHandle/position"),
    "GetWindowRect":
    (Method.GET, "/session/:sessionId/window/rect"),
    "GetWindowSize":
    (Method.GET, "/session/:sessionId/window/:windowHandle/size"),
    "GetWindows": (Method.GET, "/session/:sessionId/window_handles"),
    "GoBack": (Method.POST, "/session/:sessionId/back"),
    "GoForward": (Method.POST, "/session/:sessionId/forward"),
    "HeapSnapshot": (Method.GET, "/session/:sessionId/chromium/heap_snapshot"),
    "HoverElement": (Method.POST, "/session/:sessionId/element/:id/hover"),
    "InitSession": (Method.POST, "/session"),
    "IsAlertOpen": (Method.GET, "/session/:sessionId/alert"),
    "IsAutoReporting": (Method.GET, "/session/:sessionId/autoreport"),
    "IsElementDisplayed":
    (Method.GET, "/session/:sessionId/element/:id/displayed"),
    "IsElementEnabled": (Method.GET, "/session/:sessionId/element/:id/enabled"),
    "IsElementEqual":
    (Method.GET, "/session/:sessionId/element/:id/equals/:other"),
    "IsElementSelected":
    (Method.GET, "/session/:sessionId/element/:id/selected"),
    "IsLoading": (Method.GET, "/session/:sessionId/is_loading"),
    "LaunchApp": (Method.POST, "/session/:sessionId/chromium/launch_app"),
    "MaximizeWindow": (Method.POST, "/session/:sessionId/window/maximize"),
    "MinimizeWindow": (Method.POST, "/session/:sessionId/window/minimize"),
    "MouseDown": (Method.POST, "/session/:sessionId/buttondown"),
    "MouseMove": (Method.POST, "/session/:sessionId/moveto"),
    "MouseUp": (Method.POST, "/session/:sessionId/buttonup"),
    "Navigate": (Method.POST, "/session/:sessionId/url"),
    "PerformActions": (Method.POST, "/session/:sessionId/actions"),
    "Quit": (Method.DELETE, "/session/:sessionId"),
    "Refresh": (Method.POST, "/session/:sessionId/refresh"),
    "RemoveLocalStorageItem":
    (Method.DELETE, "/session/:sessionId/local_storage/key/:key"),
    "RemoveSessionStorageItem":
    (Method.DELETE, "/session/:sessionId/session_storage/key/:key"),
    "Resume": (Method.POST, "/session/:sessionId/goog/page/resume"),
    "Screenshot": (Method.GET, "/session/:sessionId/screenshot"),
    "SendCommand": (Method.POST, "/session/:sessionId/chromium/send_command"),
    "SendCommandAndGetResult":
    (Method.POST, "/session/:sessionId/chromium/send_command_and_get_result"),
    "SetAlertPrompt": (Method.POST, "/session/:sessionId/alert_text"),
    "SetAutoReporting": (Method.POST, "/session/:sessionId/autoreport"),
    "SetGeolocation": (Method.POST, "/session/:sessionId/location"),
    "SetImplicitWait":
    (Method.POST, "/session/:sessionId/timeouts/implicit_wait"),
    "SetLocalStorageKeys": (Method.POST, "/session/:sessionId/local_storage"),
    "SetNetworkConditions":
    (Method.POST, "/session/:sessionId/chromium/network_conditions"),
    "SetNetworkConnection":
    (Method.POST, "/session/:sessionId/network_connection"),
    "SetScreenOrientation": (Method.POST, "/session/:sessionId/orientation"),
    "SetScriptTimeout":
    (Method.POST, "/session/:sessionId/timeouts/async_script"),
    "SetSessionStorageItem":
    (Method.POST, "/session/:sessionId/session_storage"),
    "SetTimeouts": (Method.POST, "/session/:sessionId/timeouts"),
    "SetWindowPosition":
    (Method.POST, "/session/:sessionId/window/:windowHandle/position"),
    "SetWindowRect": (Method.POST, "/session/:sessionId/window/rect"),
    "SetWindowSize":
    (Method.POST, "/session/:sessionId/window/:windowHandle/size"),
    "SubmitElement": (Method.POST, "/session/:sessionId/element/:id/submit"),
    "SwitchToFrame": (Method.POST, "/session/:sessionId/frame"),
    "SwitchToParentFrame": (Method.POST, "/session/:sessionId/frame/parent"),
    "SwitchToWindow": (Method.POST, "/session/:sessionId/window"),
    "Tap": (Method.POST, "/session/:sessionId/touch/click"),
    "TouchDoubleTap": (Method.POST, "/session/:sessionId/touch/doubleclick"),
    "TouchDown": (Method.POST, "/session/:sessionId/touch/down"),
    "TouchFlick": (Method.POST, "/session/:sessionId/touch/flick"),
    "TouchLongPress": (Method.POST, "/session/:sessionId/touch/longclick"),
    "TouchMove": (Method.POST, "/session/:sessionId/touch/move"),
    "TouchPinch": (Method.POST, "/session/:sessionId/touch/pinch"),
    "TouchScroll": (Method.POST, "/session/:sessionId/touch/scroll"),
    "TouchUp": (Method.POST, "/session/:sessionId/touch/up"),
    "Type": (Method.POST, "/session/:sessionId/keys"),
    "TypeElement": (Method.POST, "/session/:sessionId/element/:id/value"),
    "UploadFile": (Method.POST, "/session/:sessionId/file")
}

MULTI_SESSION_COMMANDS = ["GetSessions"]


class ReplayException(Exception):
  """Thrown for irrecoverable problems in parsing the log file."""


def _CountChar(line, opening_char, closing_char):
  """Count (number of opening_char) - (number of closing_char) in |line|.

  Used to check for the end of JSON parameters. Ignores characters inside of
  non-escaped quotes.

  Args:
    line: line to count characters in
    opening_char: "+1" character, { or [
    closing_char: "-1" character, ] or }
  Returns:
    (number of opening_char) - (number of closing_char)
  """
  in_quote = False
  total = 0
  for i, c in enumerate(line):
    if not in_quote and c is opening_char:
      total += 1
    if not in_quote and c is closing_char:
      total -= 1
    if c == '"' and (i == 0 or line[i-1] != "\\"):
      in_quote = not in_quote

  return total


def _GetCommandName(header_line):
  """Return the command name from the logged header line."""
  return header_line.split()[3]


def _GetEntryType(header_line):
  return header_line.split()[2]


def _GetSessionId(header_line):
  """Return the session ID from the logged header line."""
  return header_line.split()[1][1:-1]


# TODO(cwinstanley): Might just want to literally dump these to strings and
# search using regexes. All the ids have distinctive formats
# and this would allow getting even ids returned from scripts.
# TODO(cwinstanley): W3C element compliance
def _GetAnyElementIds(payload):
  """Looks for any element, session, or window IDs, and returns them.

  Payload should be passed as a dict or list.

  Args:
    payload: payload to check for IDs, as a python list or dict.
  Returns:
    list of ID strings, in order, in this payload
  """
  if isinstance(payload, dict):
    if "ELEMENT" in payload:
      return [payload["ELEMENT"]]
  elif isinstance(payload, list):
    elements = [item["ELEMENT"] for item in payload if "ELEMENT" in item]
    windows = [item for item in payload if "CDwindow" in item]
    if not elements and not windows:
      return None

    return elements + windows

  return None


def _ReplaceWindowAndElementIds(payload, id_map):
  """Replace the window, session, and element IDs in |payload| using |id_map|.

  Checks |payload| for window, element, and session IDs that are in |id_map|,
  and replaces them.

  Args:
    payload: payload in which to replace IDs. This is edited in-place.
    id_map: mapping from old to new IDs that should be replaced.
  """
  if isinstance(payload, dict):
    for key, value in payload.iteritems():
      if isinstance(value, basestring) and value in id_map:
        payload[key] = id_map[value]
      else:
        _ReplaceWindowAndElementIds(payload[key], id_map)
  elif isinstance(payload, list):
    for i, value in enumerate(payload):
      if isinstance(value, basestring) and value in id_map:
        payload[i] = id_map[value]
      else:
        _ReplaceWindowAndElementIds(payload[i], id_map)


def _ReplaceUrl(payload, base_url):
  """Swap out the base URL (starting with protocol) in this payload.

  Useful when switching ports or URLs.

  Args:
    payload: payload in which to do the url replacement
    base_url: url to replace any applicable urls in |payload| with.
  """
  if base_url and "url" in payload:
    payload["url"] = re.sub(r"^https?://((?!/).)*/",
                            base_url + "/", payload["url"])


def _ReplaceBinary(payload, binary):
  """Replace the binary path in |payload| with the one in |binary|.

  If |binary| exists but there is no binary in |payload|, it is added at the
  appropriate location. Operates in-place.

  Args:
    payload: InitSession payload as a dictionary to replace binary in
    binary: new binary to replace in payload. If binary is not truthy, but
    there is a binary path in |payload|, we remove the binary path, which will
    trigger ChromeDriver's mechanism for locating the Chrome binary.
  """
  if ("desiredCapabilities" in payload
      and "chromeOptions" in payload["desiredCapabilities"]):
    if binary:
      (payload["desiredCapabilities"]["chromeOptions"]
       ["binary"]) = binary
    elif "binary" in payload["desiredCapabilities"]["chromeOptions"]:
      del payload["desiredCapabilities"]["chromeOptions"]["binary"]

  elif binary:
    if "desiredCapabilities" not in payload:
      payload["desiredCapabilities"] = {
          "chromeOptions": {
              "binary": binary
          }
      }
    elif "chromeOptions" not in payload["desiredCapabilities"]:
      payload["desiredCapabilities"]["chromeOptions"] = {
          "binary": binary
      }


def _ReplaceSessionId(payload, id_map):
  """Update session IDs in this payload to match the current session.

  Operates in-place.

  Args:
    payload: payload in which to replace session IDs.
    id_map: mapping from logged IDs to IDs in the current session
  """
  if "sessionId" in payload and payload["sessionId"] in id_map:
    payload["sessionId"] = id_map[payload["sessionId"]]


class _Payload(object):
  """Object containing a payload, which usually belongs to a LogEntry."""

  def __init__(self, payload_string):
    """Initialize the payload object.

    Parses the payload, represented as a string, into a Python object.

    Payloads appear in the log as a multi-line (usually) JSON string starting
    on the header line, like the following, where the payload starts after the
    word InitSession:
    [1532467931.153][INFO]: [<session_id>] COMMAND InitSession {
       "desiredCapabilities": {
          "chromeOptions": {
             "args": [ "no-sandbox", "disable-gpu" ],
             "binary": "<binary_path>"
          }
       }
    }

    Payloads can also be "singular" entries, like "1", "false", be an error
    string (signified by the payload starting with "ERROR") or be totally
    nonexistent for a given command.

    Args:
      payload_string: payload represented as a string.
    """
    self.is_empty = not payload_string
    self.is_error = not self.is_empty and payload_string[:5] == "ERROR"
    if self.is_error or self.is_empty:
      self.payload_raw = payload_string
    else:
      self.payload_raw = json.loads(payload_string)

  def AddSessionId(self, session_id):
    """Adds a session ID into this payload.

    Args:
      session_id: session ID to add.
    """
    self.payload_raw["sessionId"] = session_id

  def SubstituteIds(self, id_map, binary, base_url="", init_session=False):
    """Replace old IDs in the given payload with ones for the current session.

    Args:
      id_map: mapping from logged IDs to current-session ones
      binary: binary to add into this command, if |init_session| is True
      base_url: base url to replace in the payload for navigation commands
      init_session: whether this payload belongs to an InitSession command.
    """
    if self.is_error or self.is_empty:
      return

    _ReplaceWindowAndElementIds(self.payload_raw, id_map)
    _ReplaceSessionId(self.payload_raw, id_map)

    if init_session:
      _ReplaceBinary(self.payload_raw, binary)

    _ReplaceUrl(self.payload_raw, base_url)

  def GetAnyElementIds(self):
    return _GetAnyElementIds(self.payload_raw)


class _GetSessionsResponseEntry(object):
  """Special LogEntry object for GetSessions commands.

  We need a separate class for GetSessions because we need to manually build
  the payload from separate log entries in CommandSequence._HandleGetSessions.
  This means that we cannot use the payload object that we use for other
  commands. There is also no canonical session ID for GetSessions.
  """

  def __init__(self, payload):
    """Initialize the _GetSessionsResponseEntry.

    Args:
      payload: python dict of the payload for this GetSessions response
    """
    self._payload = payload
    self.name = "GetSessions"
    self.session_id = ""

  def GetPayloadPrimitive(self):
    """Get the payload for this entry."""
    return self._payload


class LogEntry(object):
  """A helper class that can store a command or a response.

  Public attributes:
    name: name of the command, like InitSession.
    session_id: session ID for this command, let as "" for GetSessions.
    payload: parameters for a command or the payload returned with a response.
  """

  _COMMAND = "COMMAND"
  _RESPONSE = "RESPONSE"

  def __init__(self, header_line, payload_string):
    """Initialize the LogEntry.

    Args:
      header_line: the line from the log that has the header of this entry.
        This also sometimes has part or all of the payload in it.

        Header lines look like the following:
        [1532467931.153][INFO]: [<session_id>] <COMMAND or RESPONSE> <command>
      payload_string: string representing the payload (usually a JSON dict, but
        occasionally a string, bool, or int).
    """
    self.name = _GetCommandName(header_line)
    self._type = _GetEntryType(header_line)
    self.session_id = _GetSessionId(header_line)
    self.payload = _Payload(payload_string)

  def IsResponse(self):
    """Returns whether this instance is a response."""
    return self._type == self._RESPONSE

  def IsCommand(self):
    """Returns whether this instance is a command."""
    return self._type == self._COMMAND

  def UpdatePayloadForReplaySession(self,
                                    id_map=None,
                                    binary="",
                                    base_url=None):
    """Processes IDs in the payload to match the current session.

    This replaces old window, element, and session IDs in the payload to match
    the ones in the current session as defined in |id_map|. It also replaces
    the binary and the url if appropriate.

    Args:
      id_map:
        dict matching element, session, and window IDs in the logged session
        with the ones from the current (replaying) session.
      binary:
        Chrome binary to replace if this is an InitSession call. The binary
        will be removed if this is not set. This will cause ChromeDriver to
        use it's own algorithm to find an appropriate Chrome binary.
      base_url:
        Url to replace the ones in the log with in Navigate commands.
    """
    self.payload.AddSessionId(self.session_id)
    self.payload.SubstituteIds(
        id_map, binary, base_url, self.name == "InitSession")

  def GetPayloadPrimitive(self):
    """Returns the payload associated with this LogEntry as a primitive."""
    return self.payload.payload_raw


class _ParserWithUndo(object):
  def __init__(self, log_file):
    """Wrapper around _Parser that implements a UndoGetNext function.

    Args:
      log_file: file that we wish to open as the log. This should be a
        Python file object, or something else with readline capability.
    """
    self._parser = _Parser(log_file)
    self._saved_log_entry = None

  def GetNext(self):
    """Get the next client command or response in the log.

    Returns:
      LogEntry object representing the next command or response in the log.
    """
    if self._saved_log_entry is not None:
      log_entry = self._saved_log_entry
      self._saved_log_entry = None
      return log_entry
    return self._parser.GetNext()

  def UndoGetNext(self, log_entry):
    """Undo the most recent GetNext call that returned |log_entry|.

    Simulates going backwards in the log file by storing |log_entry| and
    returning that on the next GetNext call.

    Args:
      entry: the returned entry from the GetNext that we wish to "undo"
    Raises:
      ReplayException: if this is called multiple times in a row, which will
        cause the object to lose the previously undone entry.
    """
    if self._saved_log_entry is not None:
      raise RuntimeError('Cannot undo multiple times in a row.')
    self._saved_log_entry = log_entry


class _Parser(object):
  """Class responsible for parsing (and not interpreting) the log file."""

  # Matches headers for client commands/responses only (not DevTools events)
  _CLIENT_PREAMBLE_REGEX = re.compile(
      r"^\[[0-9]{10}\.[0-9]{3}\]\[INFO\]: \[[a-f0-9]*\]")

  def __init__(self, log_file):
    """Initialize the _Parser instance.

    Args:
      log_file: file that we wish to open as the log. This should be a
        Python file object, or something else with readline capability.
    """
    self._log_file = log_file

  def GetNext(self):
    """Get the next client command or response in the log.

    Returns:
      LogEntry object representing the next command or response in the log.
        Returns None if at the end of the log
    """
    header = self._GetNextClientHeaderLine()
    if not header:
      return None
    payload_string = self._GetPayloadString(header)
    return LogEntry(header, payload_string)

  def _GetNextClientHeaderLine(self):
    """Get the next line that is a command or response for the client.

    Returns:
      String containing the header of the next client command/response, or
        an empty string if we're at the end of the log file.
    """
    while True:
      next_line = self._log_file.readline()
      if not next_line:  # empty string indicates end of the log file.
        return None
      if re.match(self._CLIENT_PREAMBLE_REGEX, next_line):
        return next_line

  def _GetPayloadString(self, header_line):
    """Gets the payload for the current command in self._logfile.

    Parses the given header line, along with any additional lines as
    applicable, to get a complete JSON payload object from the current
    command in the log file. Note that the payload can be JSON, and error
    (just a string), or something else like an int or a boolean.

    Args:
      header_line: the first line of this command
    Raises:
      ReplayException: if the JSON appears to be incomplete in the log
    Returns:
      payload of the command as a string
    """
    header_segments = header_line.split()
    if len(header_segments) < 5:
      return None
    payload = " ".join(header_segments[4:])
    opening_char = header_segments[4]
    if opening_char == "{":
      closing_char = "}"
    elif opening_char == "[":
      closing_char = "]"
    else:
      return payload  # payload is singular, like "1", "false", or an error

    opening_char_count = (payload.count(opening_char)
                          - payload.count(closing_char))

    while opening_char_count > 0:
      next_line = self._log_file.readline()
      if not next_line:
        # It'd be quite surprising that the log is truncated in the middle of
        # a JSON; far more likely that the parsing failed for some reason.
        raise ReplayException(
            "Reached end of file without reaching end of JSON payload")
      payload += next_line
      opening_char_count += _CountChar(next_line, opening_char,
                                       closing_char)

    return payload


class CommandSequence(object):
  """Interface to the sequence of commands in a log file."""

  def __init__(self, log_path="", base_url=None, chrome_binary=None):
    """Initialize the CommandSequence.

    Args:
      log_path: file to read logs from (usually opened with with)
      base_url: url to replace the base of logged urls with, if
        applicable. Replaces port number as well.
      chrome_binary: use this Chrome binary instead of the one in the log,
        if not None.
    """
    self._base_url = base_url
    self._binary = chrome_binary
    self._id_map = {}
    self._parser = _ParserWithUndo(log_path)
    self._staged_logged_ids = None
    self._staged_logged_session_id = None
    self._last_response = None

  def NextCommand(self, previous_response):
    """Get the next command in the log file.

    Gets start of next command, returning the command and response,
    ready to be executed directly in the new session.

    Args:
      previous_response: the response payload from running the previous command
        outputted by this function; None if this is the first command, or
        element, session, and window ID substitution is not desired (i.e.
        use the logged IDs). This provides the IDs that are then mapped
        back onto the ones in the log to formulate future commands correctly.
    Raises:
      ReplayException: there is a problem with the log making it not
        parseable.
    Returns:
      None if there are no remaining logs.
      Otherwise, |command|, a LogEntry object with the following fields:
        name: command name (e.g. InitSession)
        type: either LogEntry.COMMAND or LogEntry.RESPONSE
        payload: parameters passed with the command
        session_id: intended session ID for the command, or "" if the
          command is GetSessions.
    """
    if previous_response:
      self._IngestRealResponse(previous_response)

    command = self._parser.GetNext()
    if not command:  # Reached end of log file
      return None
    if not command.IsCommand():
      raise ReplayException("Command and Response unexpectedly out of order.")

    if command.name == "GetSessions":
      return self._HandleGetSessions(command)

    command.UpdatePayloadForReplaySession(
        self._id_map, self._binary, self._base_url)

    response = self._parser.GetNext()
    if not response.IsResponse():
      raise ReplayException("Command and Response unexpectedly out of order.")
    self._IngestLoggedResponse(response)
    return command

  def _IngestRealResponse(self, response):
    """Process the actual response from the previously issued command.

    Ingests the given response that came from calling the last command on
    the running ChromeDriver replay instance. This is the step where the
    session and element IDs are matched between |response| and the logged
    response.

    Args:
      response: Python dict of the real response to be analyzed for IDs.
    """
    if "value" in response and self._staged_logged_ids:
      real_ids = _GetAnyElementIds(response["value"])
      if real_ids and self._staged_logged_ids:
        for id_old, id_new in zip(self._staged_logged_ids, real_ids):
          self._id_map[id_old] = id_new
        self._staged_logged_ids = None

    if "sessionId" in response and self._staged_logged_session_id:
      self._id_map[self._staged_logged_session_id] = response["sessionId"]
      self._staged_logged_session_id = None

  def _IngestLoggedResponse(self, response):
    """Reads the response at the current position in the log file.

    Also matches IDs between the logged and new sessions.

    Args:
      response: the response from the log (from _parser.GetNext)
    """
    self._last_response = response  # store for testing purposes

    self._staged_logged_ids = response.payload.GetAnyElementIds()
    if response.name == "InitSession":
      self._staged_logged_session_id = response.session_id

  def _HandleGetSessions(self, first_command):
    """Special case handler for the GetSessions command.

    Since it is dispatched to each session thread, GetSessions doesn't guarantee
    command-response-command-response ordering in the log. This happens with
    getSessions, which is broadcast to and logged by each of the active sessions
    in the ChromeDriver instance. This simply consumes all the necessary logs
    resulting from that command until it reaches the next command in the log.

    This results in one returned |overall_response|, which is a list of the
    responses from each GetSessions sub-call. This is not the same as what is
    in the log file, but it is what ChromeDriver returns in real life.

    Args:
      first_command: The first GetSessions command from the log

    Returns:
      first_command: the command that triggered all of the calls absorbed by
        this function
    """

    command_response_pairs = collections.defaultdict(dict)
    command_response_pairs[first_command.session_id] = (
        {"command": first_command})

    while True:
      next_entry = self._parser.GetNext()
      if not next_entry:
        self._parser.UndoGetNext(next_entry)
        break
      if next_entry.IsResponse():
        command_response_pairs[next_entry.session_id]["response"] = next_entry
      elif next_entry.IsCommand():
        if (next_entry.name != first_command.name
            or next_entry.session_id in command_response_pairs):
          self._parser.UndoGetNext(next_entry)
          break
        command_response_pairs[next_entry.session_id]["command"] = next_entry

    response = [
        {u"id": key, u"capabilities": val["response"].GetPayloadPrimitive()}
        for key, val in command_response_pairs.iteritems()
    ]
    self._last_response = _GetSessionsResponseEntry(response)

    return first_command


class Replayer(object):
  """Replays the commands in the log file, using CommandSequence internally.

  This class provides the command-line functionality for this file.
  """

  def __init__(self, logfile, server, chrome_binary, base_url=None):
    """Initialize the Replayer instance.

    Args:
      logfile: log file handle object to replay from.
      options: command-line options; see below. Needs at least
        options.chromedriver for the ChromeDriver binary.
      base_url: string, base of the url to replace in the logged urls (useful
      for when ports change). If any value is passed here, it overrides any
        base url passed in options.
    """


    # TODO(cwinstanley) Add Android support and perhaps support for other
    # chromedriver command line options.
    self.executor = command_executor.CommandExecutor(server.GetUrl())
    self.command_sequence = CommandSequence(logfile, base_url=base_url,
                                            chrome_binary=chrome_binary)

  def Run(self):
    """Runs the replay."""
    real_response = None
    while True:
      command = self.command_sequence.NextCommand(real_response)
      if not command:
        break
      real_response = self.executor.Execute(_COMMANDS[command.name],
                                            command.GetPayloadPrimitive())


def StartChromeDriverServer(chromedriver_binary,
                            output_log_path,
                            devtools_replay_path="",
                            replayable=False):
  chromedriver = util.GetAbsolutePathOfUserPath(chromedriver_binary)
  if (not os.path.exists(chromedriver) and
      util.GetPlatformName() == "win" and
      not chromedriver.lower().endswith(".exe")):
    chromedriver = chromedriver + ".exe"
  if output_log_path:
    output_log_path = util.GetAbsolutePathOfUserPath(output_log_path)

  chromedriver_server = server.Server(chromedriver_binary,
                                      log_path=output_log_path,
                                      devtools_replay_path=devtools_replay_path,
                                      replayable=replayable)

  return chromedriver_server


def _GetCommandLineOptions():
  """Get, parse, and error check command line options for this file."""
  usage = "usage: %prog <chromedriver binary> <input log path> [options]"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option(
      "", "--output-log-path",
      help="Output verbose server logs to this file")
  parser.add_option(
      "", "--chrome", help="Path to a build of the chrome binary. If not\n"
      "specified, uses ChromeDriver's own algorithm to find Chrome.")
  parser.add_option(
      "", "--base-url", help="Base url to replace logged urls (in "
      "navigate, getUrl, and similar commands/responses).")
  parser.add_option(
      "", "--devtools-replay", help="Replay DevTools actions in addition\n"
      "to client-side actions")
  # TODO(crbug.com/chromedriver/2501)
  parser.add_option(
      "", "--replayable", help="Generate logs that do not have truncated\n"
      "strings so that they can be replayed again.")

  options, args = parser.parse_args()
  if not os.path.exists(args[0]):
    parser.error("Path given for chromedriver is invalid.\n"
                 'Please run "%s --help" for help' % __file__)
  if options.chrome and not os.path.exists(options.chrome):
    parser.error("Path given by --chrome is invalid.\n"
                 'Please run "%s --help" for help' % __file__)
  if options.replayable and not options.output_log_path:
    parser.error("Replayable log option needs --output-log-path specified. \n"
                 'Please run "%s --help" for help' % __file__)

  return options, args


def main():
  options, args = _GetCommandLineOptions()
  devtools_replay_path = args[1] if options.devtools_replay else None
  server = StartChromeDriverServer(args[0], options.output_log_path,
      devtools_replay_path, options.replayable)
  input_log_path = util.GetAbsolutePathOfUserPath(args[1])
  chrome_binary = (util.GetAbsolutePathOfUserPath(options.chrome)
                   if options.chrome else None)

  with open(input_log_path) as logfile:
    Replayer(logfile, server, chrome_binary, options.base_url).Run()

  server.Kill()


if __name__ == "__main__":
  main()
