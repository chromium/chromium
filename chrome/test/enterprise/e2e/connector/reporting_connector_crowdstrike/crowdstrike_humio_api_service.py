# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import requests
from .. import Verifyable, VerifyContent


class CrowdStrikeHumioApiService(Verifyable):

  def __init__(self, user_token):
    '''Inits the api client with base url, user auth token and repo name.'''
    self.base_url = 'https://cloud.us.humio.com'
    self.user_token = user_token
    self.repository = 'demo'

  def TryVerify(self, content: VerifyContent) -> bool:
    '''Makes HTTP POST call with user token and search device_id from humio.

    API reference: https://library.humio.com/humio-server/api-search.html'''
    headers = {
        "Authorization": "Bearer " + self.user_token,
        "Content-Type": "application/json",
        "Accept": "application/json"
    }
    data = {
        "queryString": content.device_id,
        "start": "1h",
        "showQueryEventDistribution": True,
        "isLive": False
    }
    response = requests.post(
        self.base_url + '/api/v1/repositories/' + self.repository + '/query',
        json=data,
        headers=headers)
    logging.info(len(response.json()))
    return len(response.json()) > 0
