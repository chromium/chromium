# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os

from googleapiclient.discovery import build
from google.oauth2 import service_account
from .. import Verifyable, VerifyContent


class RealTimeReportingServer(Verifyable):
  SCOPES = ['https://www.googleapis.com/auth/admin.reports.audit.readonly']
  USER_EMAIL = 'admin@beyondcorp.bigr.name'

  def __init__(self, credentials):
    self.credentials = None
    try:
      serviceAccountInfo = json.loads(credentials)
      self.credentials = service_account.Credentials.from_service_account_info(
          serviceAccountInfo, scopes=self.SCOPES)
    except json.JSONDecodeError:
      print('Decoding RealTimeReportingServer credentials JSON has failed')

  def create_reports_service(self, user_email):
    """Build and returns an Admin SDK Reports service object authorized with
      the service accounts that act on behalf of the given user.

      Args:
        user_email: The email of the user. Needs permissions to access
        the Admin APIs.
      Returns:
        Admin SDK reports service object.
      """
    delegatedCreds = self.credentials.with_subject(user_email)

    return build('admin', 'reports_v1', credentials=delegatedCreds)

  def TryVerify(self, content: VerifyContent) -> bool:
    """This method will be called repeatedly until
        success or timeout. Returns boolean"""
    containsEvent = False
    reportService = self.create_reports_service(self.USER_EMAIL)
    results = reportService.activities().list(
        userKey='all',
        applicationName='chrome',
        customerId='C029rpj4z',
        eventName='UNSAFE_SITE_VISIT',
        startTime=content.timestamp.strftime(
            '%Y-%m-%dT%H:%M:%S.%fZ')).execute()
    activities = results.get('items', [])
    for activity in activities:
      for event in activity.get('events', []):
        for parameter in event.get('parameters', []):
          if parameter['name'] == 'DEVICE_ID' and \
          parameter['value'] in content.device_id:
            containsEvent = True
            break
    return containsEvent
