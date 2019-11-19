// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the Material Design user manager page. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/chrome_features.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function UserManagerBrowserTest() {}

UserManagerBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-user-manager/',

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '../test_browser_proxy.js',
    'control_bar_tests.js',
    'create_profile_tests.js',
    'test_profile_browser_proxy.js',
    'user_manager_pages_tests.js',
  ],
};

GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_UserManagerTest DISABLED_UserManagerTest');
GEN('#else');
GEN('#define MAYBE_UserManagerTest UserManagerTest');
GEN('#endif');
TEST_F('UserManagerBrowserTest', 'MAYBE_UserManagerTest', function() {
  user_manager.control_bar_tests.registerTests();
  user_manager.create_profile_tests.registerTests();
  user_manager.user_manager_pages_tests.registerTests();
  mocha.run();
});
