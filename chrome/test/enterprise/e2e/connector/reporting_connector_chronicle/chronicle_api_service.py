# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime, timedelta
import json
from google.oauth2 import service_account
from googleapiclient import _auth
from .. import Verifyable, VerifyContent


class ChronicleApiService(Verifyable):
  """This class handles retrieving and verifying chronicle messages"""
  messages = []
  serviceAccountInfo = None

  def __init__(self, credentials):
    """Construct a ChronicleApiService instance.

    Args:
      credentials: the account key details.
    """
    try:
      self.serviceAccountInfo = json.loads(credentials)
    except json.JSONDecodeError:
      print('Decoding PubsubApiService credentials JSON has failed')

  def TryVerify(self, content: VerifyContent) -> bool:
    """This method will be called repeatedly until
        success or timeout. Returns boolean"""
    self.loadEvents(content.timestamp, content.device_id)
    return content.device_id in json.dumps(self.messages)

  def _datetimeToIso(self, date):
    return date.strftime('%Y-%m-%dT%H:%M:%S.%f%zZ')

  def loadEvents(self, testStartTime, deviceId):
    """Calls Chronicle API to fetch events

    Args:
      deviceId: A GUID device id that made the action.
      testStartTime: A datetime of the start time for the events
    """
    # Create a credential using Google Developer Service
    # Account Credential and Chronicle API scope.
    SCOPES = ['https://www.googleapis.com/auth/chronicle-backstory']
    credentials = service_account.Credentials.from_service_account_info(
        self.serviceAccountInfo, scopes=SCOPES)

    # Build an HTTP client that can make authorized OAuth requests.
    http_client = _auth.authorized_http(credentials)

    # Construct the URL
    RESULT_EVENTS_KEY = 'events'
    BACKSTORY_API_V1_URL = 'https://backstory.googleapis.com/v1'
    start_time = self._datetimeToIso(testStartTime - timedelta(minutes=15))
    end_time = self._datetimeToIso(datetime.utcnow())
    list_event_url = (
        '{}/events:udmSearch?query=principal.resource.id+%3D+%22{}%22'
        '&time_range.start_time={}&time_range.end_time={}'
        '&limit=100').format(BACKSTORY_API_V1_URL, deviceId, start_time,
                             end_time)

    # Make a request
    print('GET', list_event_url)
    response = http_client.request(list_event_url, 'GET')

    # Parse the response
    if response[0].status == 200:
      events = response[1]
      # List of events returned for further processing
      try:
        events = json.loads(events.decode("utf-8"))
      except json.JSONDecodeError:
        print('Decoding chronicle events JSON response has failed')

      self.messages = events.get(RESULT_EVENTS_KEY, [])
    else:
      # Something went wrong, please see the response detail
      err = response[1]
      print(err)
