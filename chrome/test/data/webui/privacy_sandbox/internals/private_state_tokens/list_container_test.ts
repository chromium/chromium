// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {CrCollapseElement, PrivateStateTokensListContainerElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {dummyListItemData} from './test_data.js';

suite('ListContainerTest', () => {
  let container: PrivateStateTokensListContainerElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    container = document.createElement('private-state-tokens-list-container');
    document.body.appendChild(container);
    container.data = dummyListItemData;
    await microtasksFinished();
  });

  test('check layout', () => {
    assertTrue(isVisible(container));
    const renderedItems = container.shadowRoot!.querySelectorAll(
        'private-state-tokens-list-item');
    assertEquals(dummyListItemData.length, renderedItems.length);
  });

  test('check expand collapse', async () => {
    assertEquals(
        'Expand All', container.$.expandCollapseButton.textContent!.trim());
    const renderedItems = container.shadowRoot!.querySelectorAll(
        'private-state-tokens-list-item');
    assertEquals(3, renderedItems.length);

    const expandedContent0 =
        $$<CrCollapseElement>(renderedItems[0]!, '#expandedContent');
    assertTrue(!!expandedContent0);
    assertFalse(isVisible(expandedContent0));
    assertFalse(expandedContent0.opened);

    const expandedContent1 =
        $$<CrCollapseElement>(renderedItems[1]!, '#expandedContent');
    assertTrue(!!expandedContent1);
    assertFalse(isVisible(expandedContent1));
    assertFalse(expandedContent1.opened);

    container.$.expandCollapseButton.click();
    await microtasksFinished();

    // Verify that all items were opened when button clicked
    assertEquals(
        'Collapse All', container.$.expandCollapseButton.textContent!.trim());
    assertTrue(expandedContent0.opened);

    assertTrue(expandedContent1.opened);

    container.$.expandCollapseButton.click();
    await microtasksFinished();

    // Verify that all items close when button clicked again
    assertEquals(
        'Expand All', container.$.expandCollapseButton.textContent!.trim());
    assertFalse(expandedContent0.opened);

    assertFalse(expandedContent1.opened);
  });

  test('check unexpanded row', () => {
    const renderedItems = container.shadowRoot!.querySelectorAll(
        'private-state-tokens-list-item');
    assertEquals(3, renderedItems.length);
    assertEquals(
        null, renderedItems[2]!.shadowRoot!.querySelector('#expandedContent'));
  });
});
