// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/js/post_message_api_client.m.js';


const serverOriginURLFilter = 'chrome://parent-access/';

class TestParentAccessAPIClient extends PostMessageAPIClient {
  constructor() {
    super(serverOriginURLFilter, null);
  }

  /**
   * @override
   */
  onInitialized() {
    this.callApiFn('onParentAccessResult', ['1234567890']);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  const parentAccessTestClient = new TestParentAccessAPIClient();
});
