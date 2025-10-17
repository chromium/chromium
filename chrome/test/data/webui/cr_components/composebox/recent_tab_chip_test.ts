// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/recent_tab_chip.js';

import type {RecentTabChipElement} from 'chrome://resources/cr_components/composebox/recent_tab_chip.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('RecentTabChipTest', function() {
  let recentTabChip: RecentTabChipElement;

  const MOCK_TAB_INFO: TabInfo = {
    tabId: 1,
    title: 'Tab 1',
    url: {url: 'https://tab1.com'},
    lastActive: {internalValue: 1n},
  };

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    recentTabChip = document.createElement('composebox-recent-tab-chip');
    document.body.appendChild(recentTabChip);
    // @ts-expect-error: Private property access for testing.
    recentTabChip.recentTab_ = MOCK_TAB_INFO;
    await microtasksFinished();
  });

  function getButton(): HTMLElement {
    const button = $$(recentTabChip, '#recentTabButton');
    if (button === null) {
      throw new Error('Recent tab button not found.');
    }
    return button;
  }

  test('is hidden when no tab suggestions', async () => {
    // @ts-expect-error: Private property access for testing.
    recentTabChip.recentTab_ = undefined;
    await microtasksFinished();
    const button = $$(recentTabChip, '#recentTabButton');
    assertEquals(null, button);
  });

  test('is visible when there are tab suggestions', () => {
    assertTrue(getButton() !== null);
  });

  test('fires event on click with correct data', async () => {
    const eventPromise = eventToPromise('add-tab-context', recentTabChip);
    const button = getButton();
    button.click();

    const event = await eventPromise;
    assertTrue(event !== null);
    assertEquals(MOCK_TAB_INFO.tabId, event.detail.id);
    assertEquals(MOCK_TAB_INFO.title, event.detail.title);
    assertEquals(MOCK_TAB_INFO.url, event.detail.url);
  });

  test('is disabled when inputsDisabled is true', async () => {
    // @ts-expect-error: Private property access for testing.
    recentTabChip.inputsDisabled_ = true;
    await microtasksFinished();

    const button = getButton();
    assertTrue(button.hasAttribute('disabled'));
  });

  test('is not disabled when inputsDisabled is false', async () => {
    // @ts-expect-error: Private property access for testing.
    recentTabChip.inputsDisabled_ = false;
    await microtasksFinished();

    const button = getButton();
    assertFalse(button.hasAttribute('disabled'));
  });

  test('has correct text and title', () => {
    const button = getButton();
    const buttonText = button.querySelector('.recent-tab-button-text');
    assertTrue(!!buttonText);
    assertEquals(
        'Ask Google about previous tab', buttonText.textContent.trim());
    assertEquals('Tab 1', button.title);
  });

  test('becomes visible when tabs are added', async () => {
    // @ts-expect-error: Private property access for testing.
    recentTabChip.recentTab_ = undefined;
    await microtasksFinished();
    assertEquals(null, $$(recentTabChip, '#recentTabButton'));

    // @ts-expect-error: Private property access for testing.
    recentTabChip.recentTab_ = MOCK_TAB_INFO;
    await microtasksFinished();
    assertTrue(getButton() !== null);
  });

  test('does not fire event when disabled', async () => {
    let eventFired = false;
    recentTabChip.addEventListener('add-tab-context', () => {
      eventFired = true;
    });

    // @ts-expect-error: Private property access for testing.
    recentTabChip.inputsDisabled_ = true;
    await microtasksFinished();

    const button = getButton();
    assertTrue(button.hasAttribute('disabled'));

    button.click();
    await microtasksFinished();

    assertFalse(eventFired);
  });

  test('has correct aria-label', () => {
    const button = getButton();
    assertEquals('Ask Google about previous tab: Tab 1', button.ariaLabel);
  });
});
