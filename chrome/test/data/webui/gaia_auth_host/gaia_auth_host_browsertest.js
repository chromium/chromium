// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for c/b/r/gaia_auth_host.
 */

GEN('#include "content/public/test/browser_test.h"');

var GaiaAuthHostBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://dummyurl';
  }

  /** @override */
  get isAsync() {
    return true;
  }
};

[['PasswordChangeAuthenticator', 'password_change_authenticator_test.js'],
 ['SamlPasswordAttributes', 'saml_password_attributes_test.js'],
 ['SamlTimestamps', 'saml_timestamps_test.js'],
 ['SamlUsernameAutofill', 'saml_username_autofill_test.js'],
].forEach(test => registerTest(...test));

// Common WebUI pattern for test registration.
function registerTest(testName, module, caseName) {
  const className = `GaiaAuthHost${testName}Test`;
  this[className] = class extends GaiaAuthHostBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://chrome-signin/test_loader.html?module=gaia_auth_host/${
          module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
