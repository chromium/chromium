# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import json
import requests
import time

from datetime import datetime
import xml.etree.ElementTree as ET
from .. import Verifyable, VerifyContent


class SplunkApiService(Verifyable):
  baseurl = ''
  userName = ''
  password = ''

  def __init__(self, credentials):
    credentialsJson = json.loads(credentials)
    self.baseurl = 'https://' + credentialsJson['ip_address'] + ':8089'
    self.userName = credentialsJson['username']
    self.password = credentialsJson['password']

  def get_dispatch_state(self, job_id):
    response = requests.post(
        self.baseurl + '/services/search/jobs/' + job_id,
        auth=(self.userName, self.password),
        verify=False)
    root = ET.fromstring(response.text)
    print(response.text)
    dispatchState = ""
    for tag in root:
      if "content" in tag.tag:
        for tag2 in tag:
          for tag3 in tag2:
            if tag3.attrib['name'] == "dispatchState":
              dispatchState = tag3.text
    return (dispatchState.upper())

  def get_results(self, job_id):
    data = {'output_mode': 'json'}
    offset = 0
    results_fullness = False
    events = list()

    while not results_fullness:
      response = requests.get(self.baseurl + '/services/search/jobs/' \
                              + job_id + '/results?count=50000&offset=' \
                              + str(offset), data=data,
                              auth=(self.userName, self.password), verify=False)
      response_data = json.loads(response.text)
      print("Count: " + str(len(response_data['results'])))
      events += response_data['results']
      if len(response_data['results']
            ) == 0:  #This means that we got all the results
        results_fullness = True
      else:
        offset += 50000
    print("All: " + str(len(events)))
    return events

  def TryVerify(self, content: VerifyContent) -> bool:
    """This method Verify event entry existence on the splunk instance"""
    searchQuery = "sourcetype=google:chrome:json"
    minutes = int((
        (datetime.utcnow() - content.timestamp).total_seconds() / 60) + 2)
    searchQuery += ' earliest=-' + str(minutes) + 'm'
    searchQuery += ' device_id="' + content.device_id + '"'
    searchQuery += ' event="badNavigationEvent"'
    # Remove leading and trailing whitespace from the search
    searchQuery = searchQuery.strip()
    print(searchQuery)

    # If the query doesn't already start with the 'search' operator or another
    # generating command (e.g. "| inputcsv"), then prepend "search " to it.
    if not (searchQuery.startswith('search') or searchQuery.startswith("|")):
      searchQuery = 'search ' + searchQuery

    data = {'search': searchQuery, 'max_count': '10000000'}
    # Run the search.
    # disable SSL cert validation.
    response = requests.post(
        self.baseurl + '/services/search/jobs',
        data=data,
        auth=(self.userName, self.password),
        verify=False)

    root = ET.fromstring(response.text)
    for tag in root:
      job_id = tag.text

    status = "UNKNOWN"
    while status != "DONE":
      status = self.get_dispatch_state(job_id)
      print(status)
      if status not in ["QUEUED", "PARSING", "RUNNING", "FINALIZING", "DONE"]:
        return False
      time.sleep(5)

    events = self.get_results(job_id)
    return len(events) != 0
