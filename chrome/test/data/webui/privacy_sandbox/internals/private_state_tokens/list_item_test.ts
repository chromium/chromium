// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {PrivateStateTokensListItemElement, Redemption} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

const testRedemptionsDummyData: Redemption[] = [
  {origin: 'https://a.test', formattedTimestamp: 'test A'},
  {origin: 'https://b.test', formattedTimestamp: 'test B'},
];

suite('ListItemTest', () => {
  let listItem: PrivateStateTokensListItemElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    listItem = document.createElement('private-state-tokens-list-item');
    document.body.appendChild(listItem);
    listItem.redemptions = testRedemptionsDummyData;
    await microtasksFinished();
  });

  test('check layout', () => {
    assertTrue(isVisible(listItem));
    assertFalse(listItem.$.expandedContent.opened);
  });

  test('check expanded row content', async () => {
    listItem.$.expandButton.click();
    await microtasksFinished();
    assertTrue(listItem.$.expandedContent.opened);
    const redemptions = Array.from(listItem.$.expandedContent.children);
    assertEquals(testRedemptionsDummyData.length, redemptions.length);
    for (const redemption of redemptions) {
      assertTrue(isVisible(redemption));
    }
  });
});
