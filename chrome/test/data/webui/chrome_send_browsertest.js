// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests to ensure that chrome.send mocking works as expected.
 * @author scr@chromium.org (Sheridan Rawlins)
 * @see test_api.js
 */

GEN('#include "chrome/test/data/webui/chrome_send_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for chrome send WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function ChromeSendWebUITest() {}

ChromeSendWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Generate a real C++ class; don't typedef.
   * @type {?string}
   * @override
   */
  typedefCppFixture: null,

  /** @inheritDoc */
  browsePreload: DUMMY_URL,
};

// Test that chrome.send can be mocked outside the preLoad method.
TEST_F('ChromeSendWebUITest', 'NotInPreload', function() {
  let invoked = false;
  registerMessageCallback('checkSend', undefined, () => {
    invoked = true;
  });
  chrome.send('checkSend');
  assertTrue(invoked);
});
