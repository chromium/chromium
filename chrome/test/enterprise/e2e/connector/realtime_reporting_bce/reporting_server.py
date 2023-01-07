# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from googleapiclient.discovery import build
from google.oauth2 import service_account


class RealTimeReportingServer():
  SCOPES = ['https://www.googleapis.com/auth/admin.reports.audit.readonly']
  USER_EMAIL = 'admin@beyondcorp.bigr.name'

  def create_reports_service(self, user_email):
    """Build and returns an Admin SDK Reports service object authorized with
      the service accounts that act on behalf of the given user.

      Args:
        user_email: The email of the user. Needs permissions to access
        the Admin APIs.
      Returns:
        Admin SDK reports service object.
      """
    localDir = os.path.dirname(os.path.abspath(__file__))
    filePath = os.path.join(localDir, 'service_accountkey.json')
    credentials = service_account.Credentials.from_service_account_file(
        filePath, scopes=self.SCOPES)

    delegatedCreds = credentials.with_subject(user_email)

    return build('admin', 'reports_v1', credentials=delegatedCreds)

  def lookupevents(self, eventName, startTime, deviceId):
    containsEvent = False
    reportService = self.create_reports_service(self.USER_EMAIL)
    results = reportService.activities().list(
        userKey='all',
        applicationName='chrome',
        customerId='C029rpj4z',
        eventName=eventName,
        startTime=startTime).execute()
    activities = results.get('items', [])
    for activity in activities:
      for event in activity.get('events', []):
        for parameter in event.get('parameters', []):
          if parameter['name'] == 'DEVICE_ID' and \
          parameter['value'] in deviceId:
            containsEvent = True
            break
    return containsEvent
