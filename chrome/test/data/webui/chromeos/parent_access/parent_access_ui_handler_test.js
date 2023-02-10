// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ParentAccessServerMessageType, ParentAccessUIHandler} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';

window.parent_access_ui_handler_tests = {};
parent_access_ui_handler_tests.suiteName = 'ParentAccessUIHandlerTest';

/** @enum {string} */
parent_access_ui_handler_tests.TestNames = {
  TestOnParentAccessCallbackReceived:
      'tests that parent access callback was parsed',
};

suite(parent_access_ui_handler_tests.suiteName, function() {
  const parentAccessUIHandler = ParentAccessUIHandler.getRemote();
  test(
      parent_access_ui_handler_tests.TestNames
          .TestOnParentAccessCallbackReceived,
      async function() {
        // Test with an unparsable/invalid result.
        let result = await parentAccessUIHandler.onParentAccessCallbackReceived(
            'INVALID_PARENT_ACCESS_RESULT');
        assertEquals(ParentAccessServerMessageType.kError, result.message.type);

        // Decodes to a valid parent_access_callback with OnParentVerified.
        const on_verified_parent_access_callback =
            'ChwKGgoSVkFMSURfQUNDRVNTX1RPS0VOEgQIPBA8';

        result = await parentAccessUIHandler.onParentAccessCallbackReceived(
            on_verified_parent_access_callback);

        assertEquals(
            ParentAccessServerMessageType.kParentVerified, result.message.type);

        // Decodes to ignore OnConsentDeclined.
        const on_consent_declined_parent_access_callback = 'EgA=';

        result = await parentAccessUIHandler.onParentAccessCallbackReceived(
            on_consent_declined_parent_access_callback);

        assertEquals(
            ParentAccessServerMessageType.kIgnore, result.message.type);
      });
});
