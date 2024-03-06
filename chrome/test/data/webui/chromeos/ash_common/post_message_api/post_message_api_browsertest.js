// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

function PostMessageApiModuleTest() {}

PostMessageApiModuleTest.prototype = {

  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload:
      'chrome://chrome-signin/test_loader.html?module=chromeos/ash_common/post_message_api/post_message_api_test.js',

  isAsync: true,
};

// Test the postmessage api server and client by defining server methods, and
// passing a series of postmessages.
TEST_F('PostMessageApiModuleTest', 'PostMessageCommTest', function() {
  runMochaTest('PostMessageApiModuleTest', 'PostMessageCommTest');
});
