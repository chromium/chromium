// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertLT} from 'chrome://webui-test/chai_assert.js';

/**
 * Gets all of the token entries on the page.
 * @return {!NodeList} Elements displaying token information.
 */
function getTokens() {
  return document.querySelector('#token-list')
      .querySelectorAll('token-list-item');
}

/**
 * Gets the expiration time displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {Date} Expiration date of the token.
 */
function getExpirationTime(tokenEntry) {
  // Full-date format has 'at' between date and time in en-US, but
  // ECMAScript's Date.parse cannot grok it.
  return Date.parse(tokenEntry.shadowRoot.querySelector('.expiration-time')
                        .innerText.replace(' at ', ' ')
                        .replace('\u202f', ' '));
}

/**
 * Gets the extension id displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {string} Extension Id of the token.
 */
function getExtensionId(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.extension-id').innerText;
}

/**
 * Gets the account id displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {string} Account Id of the token.
 */
function getAccountId(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.account-id').innerText;
}

/**
 * Gets the extension name displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {string} Extension Name of the token.
 */
function getExtensionName(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.extension-name').innerText;
}

/**
 * Gets the revoke button of the token entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {HTMLButtonElement} Revoke button belonging related to the token.
 */
function getRevokeButton(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.revoke-button');
}

/**
 * Gets the token ID displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {string} Token ID of the token.
 */
function getAccessToken(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.access-token').innerText;
}

/**
 * Gets the token status displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {string} Token status of the token.
 */
function getTokenStatus(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.status').innerText;
}

/**
 * Gets the token scopes displayed on the page for a given entry.
 * @param {Element} tokenEntry Display element holding token information.
 * @return {string[]} Token scopes of the token.
 */
function getScopes(tokenEntry) {
  return tokenEntry.shadowRoot.querySelector('.scope-list')
      .innerHTML.split('<br>');
}

suite('NoToken', function() {
  test('emptyTokenCache', function() {
    const tokenListEntries = getTokens();
    assertEquals(0, tokenListEntries.length);
  });
});

suite('SingleToken', function() {
  test('getAllTokens', function() {
    const tokenListEntries = getTokens();
    assertEquals(1, tokenListEntries.length);
    assertEquals('Web Store', getExtensionName(tokenListEntries[0]));
    assertEquals(
        'ahfgeienlihckogmohjhadlkjgocpleb',
        getExtensionId(tokenListEntries[0]));
    assertEquals('store_account', getAccountId(tokenListEntries[0]));
    assertEquals('store_token', getAccessToken(tokenListEntries[0]));
    assertEquals('Token Present', getTokenStatus(tokenListEntries[0]));
    assertLT(getExpirationTime(tokenListEntries[0]) - new Date(), 3600 * 1000);
    const scopes = getScopes(tokenListEntries[0]);
    assertEquals(3, scopes.length);
    assertEquals('store_scope1', scopes[0]);
    assertEquals('store_scope2', scopes[1]);
    assertEquals('', scopes[2]);
  });

  test('verifyGetters', function() {
    const tokenListEntries = document.querySelector('#token-list')
                                 .querySelectorAll('token-list-item');
    const actualTokens = getTokens();
    assertEquals(tokenListEntries.length, actualTokens.length);
    assertEquals(tokenListEntries[0], actualTokens[0]);
    assertEquals(
        getExtensionName(tokenListEntries[0]),
        tokenListEntries[0]
            .shadowRoot.querySelector('.extension-name')
            .innerText);
    assertEquals(
        getExtensionId(tokenListEntries[0]),
        tokenListEntries[0]
            .shadowRoot.querySelector('.extension-id')
            .innerText);
    assertEquals(
        getAccountId(tokenListEntries[0]),
        tokenListEntries[0].shadowRoot.querySelector('.account-id').innerText);
    assertEquals(
        getAccessToken(tokenListEntries[0]),
        tokenListEntries[0]
            .shadowRoot.querySelector('.access-token')
            .innerText);
    assertEquals(
        getTokenStatus(tokenListEntries[0]),
        tokenListEntries[0].shadowRoot.querySelector('.status').innerText);
    // Full-date format has 'at' between date and time in en-US, but
    // ECMAScript's Date.parse cannot grok it.
    assertEquals(
        getExpirationTime(tokenListEntries[0]),
        Date.parse(tokenListEntries[0]
                       .shadowRoot.querySelector('.expiration-time')
                       .innerText.replace(' at ', ' ')
                       .replace('\u202f', ' ')));
    const scopes = tokenListEntries[0]
                       .shadowRoot.querySelector('.scope-list')
                       .innerHTML.split('<br>');
    const actualScopes = getScopes(tokenListEntries[0]);
    assertEquals(scopes.length, actualScopes.length);
    for (let i = 0; i < scopes.length; i++) {
      assertEquals(scopes[i], actualScopes[i]);
    }
  });
});

suite('MultipleTokens', function() {
  test('getAllTokens', function() {
    const tokenListEntries = getTokens();
    assertEquals(2, tokenListEntries.length);
    assertEquals('', getExtensionName(tokenListEntries[0]));
    assertEquals('extension0', getExtensionId(tokenListEntries[0]));
    assertEquals('account0', getAccountId(tokenListEntries[0]));
    assertEquals('token0', getAccessToken(tokenListEntries[0]));
    assertEquals('Token Present', getTokenStatus(tokenListEntries[0]));
    assertLT(getExpirationTime(tokenListEntries[0]) - new Date(), 3600 * 1000);
    let scopes = getScopes(tokenListEntries[0]);
    assertEquals(3, scopes.length);
    assertEquals('scope_1_0', scopes[0]);
    assertEquals('scope_2_0', scopes[1]);
    assertEquals('', scopes[2]);
    assertEquals('', getExtensionName(tokenListEntries[1]));
    assertEquals('extension1', getExtensionId(tokenListEntries[1]));
    assertEquals('account1', getAccountId(tokenListEntries[1]));
    assertEquals('token1', getAccessToken(tokenListEntries[1]));
    assertEquals('Token Present', getTokenStatus(tokenListEntries[1]));
    assertLT(getExpirationTime(tokenListEntries[1]) - new Date(), 3600 * 1000);
    scopes = getScopes(tokenListEntries[1]);
    assertEquals(3, scopes.length);
    assertEquals('scope_1_1', scopes[0]);
    assertEquals('scope_2_1', scopes[1]);
    assertEquals('', scopes[2]);
  });

  test('revokeToken', function(done) {
    const tokenListBefore = getTokens();
    assertEquals(2, tokenListBefore.length);
    const tokenList = document.querySelector('#token-list');
    tokenList.addEventListener('token-removed-for-test', e => {
      const tokenListAfter = getTokens();
      assertEquals(1, tokenListAfter.length);
      assertEquals(
          getAccessToken(tokenListBefore[0]),
          getAccessToken(tokenListAfter[0]));
      done();
    });
    getRevokeButton(tokenListBefore[1]).click();
  });
});
