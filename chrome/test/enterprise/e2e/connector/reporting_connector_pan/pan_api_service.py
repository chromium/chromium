# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
from datetime import timezone
import hashlib
import json
import logging
import secrets
import string
from typing import Any, Dict, Sequence, Tuple

import requests

from .. import Verifyable
from .. import VerifyContent
from .pan_event import PanEvent


# Constants
SUCCESS = 0
PENDING = 1
IGNORE = 2  # this value is ignored


class PanApiService(Verifyable):
  """This class handles retrieving events from a Palo Auto Network server.


  API guide: https://docs-cortex.paloaltonetworks.com/r/Cortex-XDR
      /Cortex-XDR-API-Reference/Get-XQL-Query-Results-Stream
  """

  def __init__(self,
               credentials: str,
               event_limit: int = 10,
               relative_time_in_minutes: int = 60):
    """Construct a PanApiService instance.

    Args:
      credentials: the credential used to connect to Pan Api Server.
      event_limit: the number of xdr event to retrieve. Default is 10.
      relative_time_in_minutes: the backwards time range in minutes to query.
        Default is 60 minutes.
    """
    self._query_id = None
    self._events = []
    self._event_limit = event_limit
    self._relative_time = relative_time_in_minutes * 3600 * 1000
    try:
      self._credentials = json.loads(credentials)
    except json.JSONDecodeError:
      logging.debug("Decoding PanApiService credentials JSON has failed")

  def TryVerify(self, content: VerifyContent) -> bool:
    """Method to be called repeated until success or timeout."""
    event_to_query = PanEvent(
        type="badNavigationEvent",
        device_id=content.device_id,
        reason="MALWARE",
        url="http://testsafebrowsing.appspot.com/s/malware.html",
    )
    logging.info("Event to look for: %s" % event_to_query)
    match_found = False

    self.start_xdr_query()
    self.get_xdr_query_results()
    events = self.get_events()
    event_string = "\n".join(str(v) for v in events)
    logging.info(f"Events logged:\n{event_string}")
    if self.query_for_event(event_to_query):
      logging.info("Matched event found\n")
      match_found = True
    self.stop_xdr_query()
    return match_found

  def _reset_ids(self):
    """Resets all the internal ids."""
    self._query_id = None

  def _generate_headers(self) -> Dict[str, str]:
    """Generates a http headers for POST requests."""
    api_key = self._credentials["api_key"]
    api_key_id = self._credentials["api_key_id"]
    # Generate a 64 bytes random string
    nonce = "".join([
        secrets.choice(string.ascii_letters + string.digits) for _ in range(64)
    ])
    # Get the current timestamp as milliseconds.
    timestamp = int(datetime.now(timezone.utc).timestamp()) * 1000
    # Generate the auth key:
    auth_key = "%s%s%s" % (api_key, nonce, timestamp)
    # Convert to bytes object
    auth_key = auth_key.encode("utf-8")
    # Calculate sha256:
    api_key_hash = hashlib.sha256(auth_key).hexdigest()
    # Generate HTTP call headers
    return {
        "x-xdr-timestamp": str(timestamp),
        "x-xdr-nonce": nonce,
        "x-xdr-auth-id": str(api_key_id),
        "Authorization": api_key_hash,
        "Content-Type": "application/json",
    }

  def query_for_event(self, ev: PanEvent) -> bool:
    """Checks for the existence of a PanEvent."""
    for event in self._events:
      # note that event['event'] is string. so another json.loads() is need to
      # convert it to a dict
      inner_event = json.loads(event["event"])
      pe = PanEvent(
          type=inner_event["event"],
          device_id=inner_event["device_id"],
          reason=inner_event["reason"] if "reason" in inner_event else "",
          url=inner_event["url"] if "url" in inner_event else "",
      )
      if ev == pe:
        return True
    return False

  def get_events(self) -> Sequence[Any]:
    """Retrieves all the stored events."""
    return self._events

  def stop_xdr_query(self):
    """Stops a XDR query."""
    self._reset_ids()
    logging.info("Connection closed.")

  def start_xdr_query(self) -> int:
    """Starts a XDR query."""
    self._reset_ids()
    xdr_dataset = self._credentials["xdr_dataset"]
    url = (
        self._credentials["api_server"] +
        r"/public_api/v1/xql/start_xql_query/")
    headers = self._generate_headers()
    parameters = {}
    request_data = {}
    request_data["query"] = (
        r"dataset=%s | fields event, time | sort desc _time | limit %d " %
        (xdr_dataset, self._event_limit))
    request_data["tenants"] = ""
    # Only query for the last self.relative_time hours
    request_data["timeframe"] = {"relativeTime": self._relative_time}
    parameters["request_data"] = request_data

    res = requests.post(
        url=url,
        headers=headers,
        json=parameters,
    )
    j = res.json()
    if res.status_code == 200:
      logging.info("Connection started...")
      self._query_id = j["reply"]
    return res.status_code

  def get_xdr_query_results(self) -> Tuple[int, int]:
    """Gets a XDR query results."""
    if not self._query_id:
      raise RuntimeError("query_id is None. Must call start_xdr_query() first.")

    url = (
        self._credentials["api_server"] +
        r"/public_api/v1/xql/get_query_results/")
    headers = self._generate_headers()
    parameters = {}
    request_data = {}
    request_data["query_id"] = self._query_id
    request_data["pending_flag"] = False
    request_data["limit"] = self._event_limit
    request_data["format"] = "json"
    parameters["request_data"] = request_data

    res = requests.post(
        url=url,
        headers=headers,
        json=parameters,
    )
    j = res.json()
    code = IGNORE
    if res.status_code == 200:
      if j["reply"]["status"] == "PENDING":
        # PENDING status indicating query hasn't yet completed or results
        # are not yet ready to be returned. Need to execute the API call again.
        code = PENDING
      elif j["reply"]["status"] == "SUCCESS":
        if "data" in j["reply"]["results"]:
          self._events = []
          for event in j["reply"]["results"]["data"]:
            self._events.append(event)
        else:  # has > 1000 events, gives up
          raise RuntimeError(
              "No data in reply/results. Stream is not supported.")
        code = SUCCESS

    return res.status_code, code
