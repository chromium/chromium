# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from google.cloud import pubsub_v1
from .. import Verifyable, VerifyContent


class PubsubApiService(Verifyable):
  """This class handles retrieving and verifying pubsub messages"""
  project = ''
  subscription = ''
  messages = None

  def __init__(self, credentials):
    """Construct a PubsubApiService instance.

    Args:
      credentials: A json string containing the project and
        pubsub subscription names.
    """
    credentialsJson = {}
    try:
      credentialsJson = json.loads(credentials)
    except json.JSONDecodeError:
      print('Decoding PubsubApiService credentials JSON has failed')
    self.project = credentialsJson['project']
    self.subscription = credentialsJson['subscription']

  def TryVerify(self, content: VerifyContent) -> bool:
    """This method will be called repeatedly until
        success or timeout. Returns boolean"""
    self.loadEvents()
    return self.doesEventExist(content.device_id)

  def doesEventExist(self, deviceId):
    """Verifies if a specific message was sent. Lazy loads messages

    Args:
      deviceId: A GUID device id that made the action.
    """
    if self.messages is None:
      self.loadEvents()
    for msg in self.messages:
      msdData = msg
      if deviceId in msdData in msdData:
        return True
    return False

  def loadEvents(self):
    with pubsub_v1.SubscriberClient() as subscriber:
      subscription_path = subscriber.subscription_path(self.project,
                                                       self.subscription)
      response = subscriber.pull(request={
          "subscription": subscription_path,
          "max_messages": 500,
      })
      print('Loaded messages :' + str(len(response.received_messages)))
      ack_ids = [msg.ack_id for msg in response.received_messages]
      self.messages = [
          msg.message.data.decode() for msg in response.received_messages
      ]
      if ack_ids:
        subscriber.acknowledge(request={
            "subscription": subscription_path,
            "ack_ids": ack_ids,
        })
