// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/ui/webui/identity_internals_ui_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * Test C++ fixture for downloads WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function IdentityInternalsUIBrowserTest() {}

/**
 * Base fixture for Downloads WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function BaseIdentityInternalsWebUITest() {}

BaseIdentityInternalsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the downloads page & call our preLoad().
   */
  browsePreload: 'chrome://identity-internals',

  /** @override */
  typedefCppFixture: 'IdentityInternalsUIBrowserTest',

  /**
   * Gets all of the token entries on the page.
   * @return {!NodeList} Elements displaying token information.
   */
  getTokens: function() {
    return document.querySelectorAll('#token-list > div');
  },

  /**
   * Gets the expiration time displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {Date} Expiration date of the token.
   */
  getExpirationTime: function(tokenEntry) {
    // Full-date format has 'at' between date and time in en-US, but
    // ECMAScript's Date.parse cannot grok it.
    return Date.parse(tokenEntry.querySelector('.expiration-time')
        .innerText.replace(' at ', ' '));
  },

  /**
   * Gets the extension id displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Extension Id of the token.
   */
  getExtensionId: function(tokenEntry) {
    return tokenEntry.querySelector('.extension-id').innerText;
  },

  /**
   * Gets the account id displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Account Id of the token.
   */
  getAccountId: function(tokenEntry) {
    return tokenEntry.querySelector('.account-id').innerText;
  },

  /**
   * Gets the extension name displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Extension Name of the token.
   */
  getExtensionName: function(tokenEntry) {
    return tokenEntry.querySelector('.extension-name').innerText;
  },

  /**
   * Gets the revoke button of the token entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {HTMLButtonElement} Revoke button belonging related to the token.
   */
  getRevokeButton: function(tokenEntry) {
    return tokenEntry.querySelector('.revoke-button');
  },

  /**
   * Gets the token ID displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Token ID of the token.
   */
  getAccessToken: function(tokenEntry) {
    return tokenEntry.querySelector('.access-token').innerText;
  },

  /**
   * Gets the token status displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Token status of the token.
   */
  getTokenStatus: function(tokenEntry) {
    return tokenEntry.querySelector('.token-status').innerText;
  },

  /**
   * Gets the token scopes displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string[]} Token scopes of the token.
   */
  getScopes: function(tokenEntry) {
    return tokenEntry.querySelector('.scope-list')
        .innerHTML.split('<br>');
  },
};

// Test verifying chrome://identity-internals Web UI when the token cache is
// empty.
TEST_F('BaseIdentityInternalsWebUITest', 'emptyTokenCache', function() {
  const tokenListEntries = this.getTokens();
  expectEquals(0, tokenListEntries.length);
});

/**
 * Fixture for Identity Internals Web UI testing with a single token in the
 * Identity API token cache.
 * @extends {BaseIdentityInternalsWebUITest}
 * @constructor
 */
function IdentityInternalsSingleTokenWebUITest() {}

IdentityInternalsSingleTokenWebUITest.prototype = {
  __proto__: BaseIdentityInternalsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  SetupTokenCacheWithStoreApp();');
  },
};

// Test for listing a token cache with a single token. It uses a known extension
// - the Chrome Web Store, in order to check the extension name.
TEST_F('IdentityInternalsSingleTokenWebUITest', 'getAllTokens', function() {
  const tokenListEntries = this.getTokens();
  expectEquals(1, tokenListEntries.length);
  expectEquals('Web Store', this.getExtensionName(tokenListEntries[0]));
  expectEquals('ahfgeienlihckogmohjhadlkjgocpleb',
               this.getExtensionId(tokenListEntries[0]));
  expectEquals('store_account',
               this.getAccountId(tokenListEntries[0]));
  expectEquals('store_token', this.getAccessToken(tokenListEntries[0]));
  expectEquals('Token Present', this.getTokenStatus(tokenListEntries[0]));
  expectLT(this.getExpirationTime(tokenListEntries[0]) - new Date(),
           3600 * 1000);
  const scopes = this.getScopes(tokenListEntries[0]);
  expectEquals(3, scopes.length);
  expectEquals('store_scope1', scopes[0]);
  expectEquals('store_scope2', scopes[1]);
  expectEquals('', scopes[2]);
});

// Test ensuring the getters on the BaseIdentityInternalsWebUITest work
// correctly. They are implemented on the child class, because the parent does
// not have any tokens to display.
TEST_F('IdentityInternalsSingleTokenWebUITest', 'verifyGetters', function() {
  const tokenListEntries = document.querySelectorAll('#token-list > div');
  const actualTokens = this.getTokens();
  expectEquals(tokenListEntries.length, actualTokens.length);
  expectEquals(tokenListEntries[0], actualTokens[0]);
  expectEquals(this.getExtensionName(tokenListEntries[0]),
      tokenListEntries[0].querySelector('.extension-name').innerText);
  expectEquals(this.getExtensionId(tokenListEntries[0]),
      tokenListEntries[0].querySelector('.extension-id').innerText);
  expectEquals(this.getAccountId(tokenListEntries[0]),
      tokenListEntries[0].querySelector('.account-id').innerText);
  expectEquals(this.getAccessToken(tokenListEntries[0]),
      tokenListEntries[0].querySelector('.access-token').innerText);
  expectEquals(this.getTokenStatus(tokenListEntries[0]),
      tokenListEntries[0].querySelector('.token-status').innerText);
  // Full-date format has 'at' between date and time in en-US, but
  // ECMAScript's Date.parse cannot grok it.
  expectEquals(this.getExpirationTime(tokenListEntries[0]),
      Date.parse(tokenListEntries[0].querySelector('.expiration-time')
          .innerText.replace(' at ', ' ')));
  const scopes =
      tokenListEntries[0].querySelector('.scope-list').innerHTML.split('<br>');
  const actualScopes = this.getScopes(tokenListEntries[0]);
  expectEquals(scopes.length, actualScopes.length);
  for (let i = 0; i < scopes.length; i++) {
    expectEquals(scopes[i], actualScopes[i]);
  }
});

/**
 * Fixture for Identity Internals Web UI testing with multiple tokens in the
 * Identity API token cache.
 * @extends {BaseIdentityInternalsWebUITest}
 * @constructor
 */
function IdentityInternalsMultipleTokensWebUITest() {}

IdentityInternalsMultipleTokensWebUITest.prototype = {
  __proto__: BaseIdentityInternalsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  SetupTokenCache(2);');
  },
};

// Test for listing a token cache with multiple tokens. Names of the extensions
// are empty, because extensions are faked, and not present in the extension
// service.
TEST_F('IdentityInternalsMultipleTokensWebUITest', 'getAllTokens', function() {
  const tokenListEntries = this.getTokens();
  expectEquals(2, tokenListEntries.length);
  expectEquals('', this.getExtensionName(tokenListEntries[0]));
  expectEquals('extension0',
               this.getExtensionId(tokenListEntries[0]));
  expectEquals('account0',
               this.getAccountId(tokenListEntries[0]));
  expectEquals('token0', this.getAccessToken(tokenListEntries[0]));
  expectEquals('Token Present', this.getTokenStatus(tokenListEntries[0]));
  expectLT(this.getExpirationTime(tokenListEntries[0]) - new Date(),
           3600 * 1000);
  let scopes = this.getScopes(tokenListEntries[0]);
  expectEquals(3, scopes.length);
  expectEquals('scope_1_0', scopes[0]);
  expectEquals('scope_2_0', scopes[1]);
  expectEquals('', scopes[2]);
  expectEquals('', this.getExtensionName(tokenListEntries[1]));
  expectEquals('extension1',
               this.getExtensionId(tokenListEntries[1]));
  expectEquals('account1',
               this.getAccountId(tokenListEntries[1]));
  expectEquals('token1', this.getAccessToken(tokenListEntries[1]));
  expectEquals('Token Present', this.getTokenStatus(tokenListEntries[1]));
  expectLT(this.getExpirationTime(tokenListEntries[1]) - new Date(),
           3600 * 1000);
  scopes = this.getScopes(tokenListEntries[1]);
  expectEquals(3, scopes.length);
  expectEquals('scope_1_1', scopes[0]);
  expectEquals('scope_2_1', scopes[1]);
  expectEquals('', scopes[2]);
});

/**
 * Fixture for asynchronous testing of Identity Internals Web UI with multiple
 * tokens in Identity API token cache.
 * @extends {IdentityInternalsMultipleTokensWebUITest}
 * @constructor
 */
function IdentityInternalsWebUITestAsync() {}

IdentityInternalsWebUITestAsync.prototype = {
  __proto__: IdentityInternalsMultipleTokensWebUITest.prototype,

  /** @override */
  isAsync: true,
};

TEST_F('IdentityInternalsWebUITestAsync', 'revokeToken', function() {
  const tokenListBefore = this.getTokens();
  expectEquals(2, tokenListBefore.length);
  const tokenList = document.querySelector('#token-list');
  tokenList.addEventListener('token-removed-for-test', e => {
    const tokenListAfter = this.getTokens();
    expectEquals(1, tokenListAfter.length);
    expectEquals(this.getAccessToken(tokenListBefore[0]),
                 this.getAccessToken(tokenListAfter[0]));
    testDone();
  });
  this.getRevokeButton(tokenListBefore[1]).click();
});
