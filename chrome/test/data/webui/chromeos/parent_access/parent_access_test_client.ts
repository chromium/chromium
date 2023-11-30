// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {PostMessageApiClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';

// Hardcode the proto string because we do not serialize or deserialize proto
// strings in this test.
export const PROTO_STRING_FOR_TEST = 'message_proto';

const serverOriginURLFilter = 'chrome://parent-access/';

class TestParentAccessApiClient extends PostMessageApiClient {
  constructor() {
    super(serverOriginURLFilter, null);
  }

  override onInitialized() {
    // The parameter roughly matches a ParentVerified typed Mojo struct that the
    // C++ handler would return to the WebUI.
    this.callApiFn('onParentAccessResult', [PROTO_STRING_FOR_TEST]);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  new TestParentAccessApiClient();
});
