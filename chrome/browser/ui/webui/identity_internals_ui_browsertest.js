// Copyright 2013 The Chromium Authors
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
    return document.querySelector('#token-list')
        .querySelectorAll('token-list-item');
  },

  /**
   * Gets the expiration time displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {Date} Expiration date of the token.
   */
  getExpirationTime: function(tokenEntry) {
    // Full-date format has 'at' between date and time in en-US, but
    // ECMAScript's Date.parse cannot grok it.
    return Date.parse(tokenEntry.shadowRoot.querySelector('.expiration-time')
        .innerText.replace(' at ', ' ').replace('\u202f', ' '));
  },

  /**
   * Gets the extension id displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Extension Id of the token.
   */
  getExtensionId: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.extension-id').innerText;
  },

  /**
   * Gets the account id displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Account Id of the token.
   */
  getAccountId: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.account-id').innerText;
  },

  /**
   * Gets the extension name displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Extension Name of the token.
   */
  getExtensionName: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.extension-name').innerText;
  },

  /**
   * Gets the revoke button of the token entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {HTMLButtonElement} Revoke button belonging related to the token.
   */
  getRevokeButton: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.revoke-button');
  },

  /**
   * Gets the token ID displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Token ID of the token.
   */
  getAccessToken: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.access-token').innerText;
  },

  /**
   * Gets the token status displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string} Token status of the token.
   */
  getTokenStatus: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.status').innerText;
  },

  /**
   * Gets the token scopes displayed on the page for a given entry.
   * @param {Element} tokenEntry Display element holding token information.
   * @return {string[]} Token scopes of the token.
   */
  getScopes: function(tokenEntry) {
    return tokenEntry.shadowRoot.querySelector('.scope-list')
        .innerHTML.split('<br>');
  },
};

// Test verifying chrome://identity-internals Web UI when the token cache is
// empty.
TEST_F('BaseIdentityInternalsWebUITest', 'emptyTokenCache', function() {
  const tokenListEntries = this.getTokens();
  assertEquals(0, tokenListEntries.length);
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
  assertEquals(1, tokenListEntries.length);
  assertEquals('Web Store', this.getExtensionName(tokenListEntries[0]));
  assertEquals('ahfgeienlihckogmohjhadlkjgocpleb',
               this.getExtensionId(tokenListEntries[0]));
  assertEquals('store_account',
               this.getAccountId(tokenListEntries[0]));
  assertEquals('store_token', this.getAccessToken(tokenListEntries[0]));
  assertEquals('Token Present', this.getTokenStatus(tokenListEntries[0]));
  assertLT(this.getExpirationTime(tokenListEntries[0]) - new Date(),
           3600 * 1000);
  const scopes = this.getScopes(tokenListEntries[0]);
  assertEquals(3, scopes.length);
  assertEquals('store_scope1', scopes[0]);
  assertEquals('store_scope2', scopes[1]);
  assertEquals('', scopes[2]);
});

// Test ensuring the getters on the BaseIdentityInternalsWebUITest work
// correctly. They are implemented on the child class, because the parent does
// not have any tokens to display.
TEST_F('IdentityInternalsSingleTokenWebUITest', 'verifyGetters', function() {
  const tokenListEntries = document.querySelector('#token-list')
      .querySelectorAll('token-list-item');
  const actualTokens = this.getTokens();
  assertEquals(tokenListEntries.length, actualTokens.length);
  assertEquals(tokenListEntries[0], actualTokens[0]);
  assertEquals(this.getExtensionName(tokenListEntries[0]),
      tokenListEntries[0].shadowRoot.querySelector('.extension-name')
      .innerText);
  assertEquals(this.getExtensionId(tokenListEntries[0]),
      tokenListEntries[0].shadowRoot.querySelector('.extension-id').innerText);
  assertEquals(this.getAccountId(tokenListEntries[0]),
      tokenListEntries[0].shadowRoot.querySelector('.account-id').innerText);
  assertEquals(this.getAccessToken(tokenListEntries[0]),
      tokenListEntries[0].shadowRoot.querySelector('.access-token').innerText);
  assertEquals(this.getTokenStatus(tokenListEntries[0]),
      tokenListEntries[0].shadowRoot.querySelector('.status').innerText);
  // Full-date format has 'at' between date and time in en-US, but
  // ECMAScript's Date.parse cannot grok it.
  assertEquals(this.getExpirationTime(tokenListEntries[0]),
      Date.parse(
          tokenListEntries[0].shadowRoot.querySelector('.expiration-time')
          .innerText.replace(' at ', ' ').replace('\u202f', ' ')));
  const scopes =
      tokenListEntries[0].shadowRoot.querySelector('.scope-list')
          .innerHTML.split('<br>');
  const actualScopes = this.getScopes(tokenListEntries[0]);
  assertEquals(scopes.length, actualScopes.length);
  for (let i = 0; i < scopes.length; i++) {
    assertEquals(scopes[i], actualScopes[i]);
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
  assertEquals(2, tokenListEntries.length);
  assertEquals('', this.getExtensionName(tokenListEntries[0]));
  assertEquals('extension0',
               this.getExtensionId(tokenListEntries[0]));
  assertEquals('account0',
               this.getAccountId(tokenListEntries[0]));
  assertEquals('token0', this.getAccessToken(tokenListEntries[0]));
  assertEquals('Token Present', this.getTokenStatus(tokenListEntries[0]));
  assertLT(this.getExpirationTime(tokenListEntries[0]) - new Date(),
           3600 * 1000);
  let scopes = this.getScopes(tokenListEntries[0]);
  assertEquals(3, scopes.length);
  assertEquals('scope_1_0', scopes[0]);
  assertEquals('scope_2_0', scopes[1]);
  assertEquals('', scopes[2]);
  assertEquals('', this.getExtensionName(tokenListEntries[1]));
  assertEquals('extension1',
               this.getExtensionId(tokenListEntries[1]));
  assertEquals('account1',
               this.getAccountId(tokenListEntries[1]));
  assertEquals('token1', this.getAccessToken(tokenListEntries[1]));
  assertEquals('Token Present', this.getTokenStatus(tokenListEntries[1]));
  assertLT(this.getExpirationTime(tokenListEntries[1]) - new Date(),
           3600 * 1000);
  scopes = this.getScopes(tokenListEntries[1]);
  assertEquals(3, scopes.length);
  assertEquals('scope_1_1', scopes[0]);
  assertEquals('scope_2_1', scopes[1]);
  assertEquals('', scopes[2]);
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
  assertEquals(2, tokenListBefore.length);
  const tokenList = document.querySelector('#token-list');
  tokenList.addEventListener('token-removed-for-test', e => {
    const tokenListAfter = this.getTokens();
    assertEquals(1, tokenListAfter.length);
    assertEquals(this.getAccessToken(tokenListBefore[0]),
                 this.getAccessToken(tokenListAfter[0]));
    testDone();
  });
  this.getRevokeButton(tokenListBefore[1]).click();
});
