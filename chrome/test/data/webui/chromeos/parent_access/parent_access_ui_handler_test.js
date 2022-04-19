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
  TestOnParentAccessResult: 'tests that parent access result was parsed',
};

suite(parent_access_ui_handler_tests.suiteName, function() {
  const parentAccessUIHandler =
      parentAccessUi.mojom.ParentAccessUIHandler.getRemote();
  test(
      parent_access_ui_handler_tests.TestNames.TestOnParentAccessResult,
      async function() {
        // Test with an unparsable/invalid result.
        let result = await parentAccessUIHandler.onParentAccessResult(
            'INVALID_PARENT_ACCESS_RESULT');
        assertEquals(
            parentAccessUi.mojom.ParentAccessResultStatus.kError,
            result.status);

        // Decodes to a valid parent_access_callback with OnParentVerified.
        const on_verified_parent_access_callback =
            'ChwKGgoSVkFMSURfQUNDRVNTX1RPS0VOEgQIPBA8';

        result = await parentAccessUIHandler.onParentAccessResult(
            on_verified_parent_access_callback);

        assertEquals(
            parentAccessUi.mojom.ParentAccessResultStatus.kParentVerified,
            result.status);

        // Decodes to a valid parent_access_callback with OnConsentDeclined.
        const on_consent_declined_parent_access_callback = 'EgA=';

        result = await parentAccessUIHandler.onParentAccessResult(
            on_consent_declined_parent_access_callback);

        assertEquals(
            parentAccessUi.mojom.ParentAccessResultStatus.kConsentDeclined,
            result.status);
      });
});
