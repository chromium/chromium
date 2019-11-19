// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for generated tests.
 * @extends {testing.Test}
 */
function AccountMigrationWelcomeUITest() {}
let testBrowserProxy = null;

AccountMigrationWelcomeUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Define the C++ fixture class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'AccountMigrationWelcomeUITest',

  /** @override */
  extraLibraries: [
    '//chrome/test/data/webui/test_browser_proxy.js',
    'account_migration_proxy_test.js',
  ],

  /** @override */
  setUp: function() {
    testBrowserProxy = new TestAccountMigrationBrowserProxy();
    account_migration.AccountMigrationBrowserProxyImpl.instance_ =
        testBrowserProxy;
    testing.Test.prototype.setUp.call(this);
  },

  /** @override */
  testGenPreamble: function() {
    GEN('ShowDialog();');
  },

  /**
   * Tests that the dialog opened to the correct URL.
   */
  testDialogUrl: function() {
    // Remove slash at the end of URL if present.
    let url = window.location.href.replace(/\/$/, '');
    assertEquals(chrome.getVariableValue('expectedUrl'), url);
  },

  /**
   * Tests that |closeDialog| function get called after clicking
   * the cancel button
   */
  testCloseDialog: function() {
    $('cancel-button').click();
    assertEquals(1, testBrowserProxy.getCallCount('closeDialog'));
  },

  /**
   * Tests that |reauthenticateAccount| function get called with expected email
   * after clicking the migrate button.
   */
  testReauthenticateAccount: function() {
    $('migrate-button').click();
    assertEquals(1, testBrowserProxy.getCallCount('reauthenticateAccount'));
    testBrowserProxy.whenCalled('reauthenticateAccount').then(email => {
      assertEquals(chrome.getVariableValue('expectedEmail'), email);
    });
  },
};

GEN('#include "chrome/browser/ui/webui/chromeos/account_migration_welcome_ui_test.h"');
GEN('');


TEST_F('AccountMigrationWelcomeUITest', 'testDialogURL', function() {
  this.testDialogUrl();
});

TEST_F('AccountMigrationWelcomeUITest', 'testCloseDialog', function() {
  this.testCloseDialog();
});

TEST_F(
    'AccountMigrationWelcomeUITest', 'testReauthenticateAccount', function() {
      this.testReauthenticateAccount();
    });
