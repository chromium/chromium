// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {CrCollapseElement, CrExpandButtonElement, PrivateStateTokensListItemElement, Redemption} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {testListItemData} from './test_data.js';

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
    const expandButton = $$<CrExpandButtonElement>(listItem, '#expandButton');
    assertTrue(!!expandButton);
    const expandedContent = $$<CrCollapseElement>(listItem, '#expandedContent');
    assertFalse(isVisible(expandedContent));
  });

  test('check expanded row content', async () => {
    const expandButton = $$<CrExpandButtonElement>(listItem, '#expandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await microtasksFinished();
    const expandedContent = $$<CrCollapseElement>(listItem, '#expandedContent');
    assertTrue(!!expandedContent);
    assertTrue(expandedContent.opened);
    const redemptions = Array.from(expandedContent.children);
    assertEquals(testRedemptionsDummyData.length, redemptions.length);
    for (const redemption of redemptions) {
      assertTrue(isVisible(redemption));
    }
  });

  test('check if numTokens is displayed', async () => {
    listItem.issuerOrigin = testListItemData.issuerOrigin;
    listItem.numTokens = testListItemData.numTokens;
    listItem.redemptions = testListItemData.redemptions;
    await microtasksFinished();
    const rowText = $$<HTMLElement>(listItem, '#row-content');
    assertTrue(!!rowText);
    const tokenSpan =
        rowText.querySelector<HTMLElement>('.cr-padded-text > #tokenText');
    const tokenText = tokenSpan!.innerText;
    assertEquals(`${testListItemData.numTokens} token`, tokenText);
  });
});
