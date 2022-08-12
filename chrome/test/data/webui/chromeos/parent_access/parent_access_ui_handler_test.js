// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../mojo_webui_test_support.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://parent-access/parent_access_ui.mojom-lite.js';

import {assert} from 'chrome://resources/js/assert.m.js';

window.parent_access_ui_handler_tests = {};
parent_access_ui_handler_tests.suiteName = 'ParentAccessUIHandlerTest';

/** @enum {string} */
parent_access_ui_handler_tests.TestNames = {
  TestOnParentAccessCallbackReceived:
      'tests that parent access callback was parsed',
};

suite(parent_access_ui_handler_tests.suiteName, function() {
  const parentAccessUIHandler =
      parentAccessUi.mojom.ParentAccessUIHandler.getRemote();
  test(
      parent_access_ui_handler_tests.TestNames
          .TestOnParentAccessCallbackReceived,
      async function() {
        // Test with an unparsable/invalid result.
        let result = await parentAccessUIHandler.onParentAccessCallbackReceived(
            'INVALID_PARENT_ACCESS_RESULT');
        assertEquals(
            parentAccessUi.mojom.ParentAccessServerMessageType.kError,
            result.message.type);

        // Decodes to a valid parent_access_callback with OnParentVerified.
        const on_verified_parent_access_callback =
            'ChwKGgoSVkFMSURfQUNDRVNTX1RPS0VOEgQIPBA8';

        result = await parentAccessUIHandler.onParentAccessCallbackReceived(
            on_verified_parent_access_callback);

        assertEquals(
            parentAccessUi.mojom.ParentAccessServerMessageType.kParentVerified,
            result.message.type);

        // Decodes to ignore OnConsentDeclined.
        const on_consent_declined_parent_access_callback = 'EgA=';

        result = await parentAccessUIHandler.onParentAccessCallbackReceived(
            on_consent_declined_parent_access_callback);

        assertEquals(
            parentAccessUi.mojom.ParentAccessServerMessageType.kIgnore,
            result.message.type);
      });
});
