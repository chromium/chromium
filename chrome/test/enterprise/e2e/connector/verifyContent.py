# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime


class VerifyContent:
  """A class to hold data that can be verified by the Verifiable interface"""
  device_id = None
  timestamp = None

  def __init__(self, deviceId: str, timestamp: datetime):
    """Constructor to initialize the class members

        Args:
        deviceId: A GUID device id that made the action.
        timestamp: A datetime of the start time for the events
    """
    self.device_id = deviceId
    self.timestamp = timestamp
