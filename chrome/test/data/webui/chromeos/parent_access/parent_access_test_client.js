// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';


const serverOriginURLFilter = 'chrome://parent-access/';

class TestParentAccessApiClient extends PostMessageApiClient {
  constructor() {
    super(serverOriginURLFilter, null);
  }

  /**
   * @override
   */
  onInitialized() {
    // The parameter roughly matches a ParentVerified typed Mojo struct that the
    // C++ handler would return to the WebUI.
    this.callApiFn('onParentAccessResult', [{message: {type: 0}}]);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  const parentAccessTestClient = new TestParentAccessApiClient();
});
