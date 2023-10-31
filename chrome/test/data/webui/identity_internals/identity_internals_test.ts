// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertLT} from 'chrome://webui-test/chai_assert.js';

/**
 * Gets all of the token entries on the page.
 * @return Elements displaying token information.
 */
function getTokens() {
  return getRequiredElement('token-list')
      .querySelectorAll<HTMLElement>('token-list-item');
}

/**
 * Gets the expiration time displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 * @return Expiration date of the token.
 */
function getExpirationTime(tokenEntry: HTMLElement): number {
  // Full-date format has 'at' between date and time in en-US, but
  // ECMAScript's Date.parse cannot grok it.
  return Date.parse(
      tokenEntry.shadowRoot!.querySelector('.expiration-time')!.textContent!
          .replace(' at ', ' ')
          .replace('\u202f', ' '));
}

/**
 * Gets the extension id displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 */
function getExtensionId(tokenEntry: HTMLElement): string {
  return tokenEntry.shadowRoot!.querySelector('.extension-id')!.textContent!;
}

/**
 * Gets the account id displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 */
function getAccountId(tokenEntry: HTMLElement): string {
  return tokenEntry.shadowRoot!.querySelector('.account-id')!.textContent!;
}

/**
 * Gets the extension name displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 */
function getExtensionName(tokenEntry: HTMLElement): string {
  return tokenEntry.shadowRoot!.querySelector('.extension-name')!.textContent!;
}

/**
 * Gets the revoke button of the token entry.
 * @param tokenEntry Display element holding token information.
 */
function getRevokeButton(tokenEntry: HTMLElement): HTMLButtonElement {
  return tokenEntry.shadowRoot!.querySelector<HTMLButtonElement>(
      '.revoke-button')!;
}

/**
 * Gets the token ID displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 */
function getAccessToken(tokenEntry: HTMLElement): string {
  return tokenEntry.shadowRoot!.querySelector('.access-token')!.textContent!;
}

/**
 * Gets the token status displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 */
function getTokenStatus(tokenEntry: HTMLElement): string {
  return tokenEntry.shadowRoot!.querySelector('.status')!.textContent!;
}

/**
 * Gets the token scopes displayed on the page for a given entry.
 * @param tokenEntry Display element holding token information.
 */
function getScopes(tokenEntry: HTMLElement): string[] {
  return tokenEntry.shadowRoot!.querySelector('.scope-list')!.innerHTML.split(
      '<br>');
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
    const entry = tokenListEntries[0]!;
    assertEquals('Web Store', getExtensionName(entry));
    assertEquals('ahfgeienlihckogmohjhadlkjgocpleb', getExtensionId(entry));
    assertEquals('store_account', getAccountId(entry));
    assertEquals('store_token', getAccessToken(entry));
    assertEquals('Token Present', getTokenStatus(entry));
    assertLT(getExpirationTime(entry) - Date.now(), 3600 * 1000);
    const scopes = getScopes(entry);
    assertEquals(3, scopes.length);
    assertEquals('store_scope1', scopes[0]);
    assertEquals('store_scope2', scopes[1]);
    assertEquals('', scopes[2]);
  });

  test('verifyGetters', function() {
    const tokenListEntries =
        getRequiredElement('token-list')
            .querySelectorAll<HTMLElement>('token-list-item');
    const actualTokens = getTokens();
    assertEquals(tokenListEntries.length, actualTokens.length);
    const entry = tokenListEntries[0]!;
    assertEquals(entry, actualTokens[0]);
    assertEquals(
        getExtensionName(entry),
        entry.shadowRoot!.querySelector('.extension-name')!.textContent);
    assertEquals(
        getExtensionId(entry),
        entry.shadowRoot!.querySelector('.extension-id')!.textContent);
    assertEquals(
        getAccountId(entry),
        entry.shadowRoot!.querySelector('.account-id')!.textContent);
    assertEquals(
        getAccessToken(entry),
        entry.shadowRoot!.querySelector('.access-token')!.textContent);
    assertEquals(
        getTokenStatus(entry),
        entry.shadowRoot!.querySelector('.status')!.textContent);
    // Full-date format has 'at' between date and time in en-US, but
    // ECMAScript's Date.parse cannot grok it.
    assertEquals(
        getExpirationTime(entry),
        Date.parse(
            entry.shadowRoot!.querySelector('.expiration-time')!.textContent!
                .replace(' at ', ' ')
                .replace('\u202f', ' ')));
    const scopes =
        entry.shadowRoot!.querySelector('.scope-list')!.innerHTML.split('<br>');
    const actualScopes = getScopes(entry);
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
    const entry1 = tokenListEntries[0]!;
    const entry2 = tokenListEntries[1]!;
    assertEquals('', getExtensionName(entry1));
    assertEquals('extension0', getExtensionId(entry1));
    assertEquals('account0', getAccountId(entry1));
    assertEquals('token0', getAccessToken(entry1));
    assertEquals('Token Present', getTokenStatus(entry1));
    assertLT(getExpirationTime(entry1) - Date.now(), 3600 * 1000);
    let scopes = getScopes(entry1);
    assertEquals(3, scopes.length);
    assertEquals('scope_1_0', scopes[0]);
    assertEquals('scope_2_0', scopes[1]);
    assertEquals('', scopes[2]);
    assertEquals('', getExtensionName(entry2));
    assertEquals('extension1', getExtensionId(entry2));
    assertEquals('account1', getAccountId(entry2));
    assertEquals('token1', getAccessToken(entry2));
    assertEquals('Token Present', getTokenStatus(entry2));
    assertLT(getExpirationTime(entry2) - Date.now(), 3600 * 1000);
    scopes = getScopes(entry2);
    assertEquals(3, scopes.length);
    assertEquals('scope_1_1', scopes[0]);
    assertEquals('scope_2_1', scopes[1]);
    assertEquals('', scopes[2]);
  });

  test('revokeToken', function(done) {
    const tokenListBefore = getTokens();
    assertEquals(2, tokenListBefore.length);
    const tokenList = getRequiredElement('token-list');
    tokenList.addEventListener('token-removed-for-test', () => {
      const tokenListAfter = getTokens();
      assertEquals(1, tokenListAfter.length);
      assertEquals(
          getAccessToken(tokenListBefore[0]!),
          getAccessToken(tokenListAfter[0]!));
      done();
    });
    getRevokeButton(tokenListBefore[1]!).click();
  });
});
