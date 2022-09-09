// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

function WebviewManagerTest() {}

WebviewManagerTest.prototype = {

  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload:
      'chrome://chrome-signin/test_loader.html?module=webview_manager/webview_manager_test.js&host=test',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  isAsync: true,
};

TEST_F('WebviewManagerTest', 'WebviewManagerAccessTokenTest', function() {
  runMochaTest('WebviewManagerTest', 'WebviewManagerAccessTokenTest');
});

TEST_F('WebviewManagerTest', 'WebviewManagerBlockAccessTokenTest', function() {
  runMochaTest('WebviewManagerTest', 'WebviewManagerBlockAccessTokenTest');
});


TEST_F('WebviewManagerTest', 'WebviewManagerAllowRequestFnTest', function() {
  runMochaTest('WebviewManagerTest', 'WebviewManagerAllowRequestFnTest');
});
