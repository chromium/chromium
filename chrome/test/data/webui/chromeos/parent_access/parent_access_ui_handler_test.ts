// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ParentAccessServerMessageType, ParentAccessUiHandler} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ParentAccessUiHandlerTest', function() {
  const parentAccessUiHandler = ParentAccessUiHandler.getRemote();

  // Tests that the callback is parsed correctly.
  test('TestOnParentAccessCallbackReceived', async function() {
    // Test with an unparsable/invalid result.
    let result = await parentAccessUiHandler.onParentAccessCallbackReceived(
        'INVALID_PARENT_ACCESS_RESULT');
    assertEquals(ParentAccessServerMessageType.kError, result.message.type);

    // Decodes to a valid parent_access_callback with OnParentVerified.
    const on_verified_parent_access_callback =
        'ChwKGgoSVkFMSURfQUNDRVNTX1RPS0VOEgQIPBA8';

    result = await parentAccessUiHandler.onParentAccessCallbackReceived(
        on_verified_parent_access_callback);

    assertEquals(
        ParentAccessServerMessageType.kParentVerified, result.message.type);

    // Decodes to ignore OnConsentDeclined.
    const on_consent_declined_parent_access_callback = 'EgA=';

    result = await parentAccessUiHandler.onParentAccessCallbackReceived(
        on_consent_declined_parent_access_callback);

    assertEquals(ParentAccessServerMessageType.kIgnore, result.message.type);
  });
});
