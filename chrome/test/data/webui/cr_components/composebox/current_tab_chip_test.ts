// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/current_tab_chip.js';

import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {CurrentTabChipElement} from 'chrome://resources/cr_components/composebox/current_tab_chip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

type AddTabContextEvent = CustomEvent<{
  id: number,
  title: string,
  url: string,
  delayUpload: boolean,
  origin: TabUploadOrigin,
}>;

suite('CurrentTabChipTest', function() {
  let currentTabChip: CurrentTabChipElement;
  let metrics: MetricsTracker;

  const MOCK_TAB_INFO: TabInfo = {
    tabId: 1,
    title: 'Tab 1',
    url: 'https://tab1.com',
    showInCurrentTabChip: true,
    showInPreviousTabChip: false,
    lastActive: {internalValue: 1n},
  };

  setup(async () => {
    metrics = fakeMetricsPrivate();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    currentTabChip = document.createElement('composebox-current-tab-chip');
    document.body.appendChild(currentTabChip);
    currentTabChip.currentTab = MOCK_TAB_INFO;
    await microtasksFinished();
  });

  function getButton(): HTMLElement {
    const button = $$(currentTabChip, '#currentTabButton');
    if (button === null) {
      throw new Error('Current tab button not found.');
    }
    return button;
  }

  test('is hidden when no tab suggestions', async () => {
    currentTabChip.currentTab = undefined;
    await microtasksFinished();
    const button = $$(currentTabChip, '#currentTabButton');
    assertEquals(null, button);
  });

  test('is visible when there are tab suggestions', () => {
    assertTrue(getButton() !== null);
  });

  test('fires event on click with correct data', async () => {
    loadTimeData.overrideValues({
      composeboxSource: 'NewTabPage',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    currentTabChip = document.createElement('composebox-current-tab-chip');
    document.body.appendChild(currentTabChip);
    currentTabChip.currentTab = MOCK_TAB_INFO;
    await microtasksFinished();

    const eventPromise =
        eventToPromise<AddTabContextEvent>('add-tab-context', currentTabChip);
    const button = getButton();
    button.click();

    const event = await eventPromise;
    assertTrue(event !== null);
    assertEquals(MOCK_TAB_INFO.tabId, event.detail.id);
    assertEquals(MOCK_TAB_INFO.title, event.detail.title);
    assertEquals(MOCK_TAB_INFO.url, event.detail.url);
    assertFalse(event.detail.delayUpload);
    assertEquals(TabUploadOrigin.CURRENT_TAB_CHIP, event.detail.origin);
    assertEquals(
        1, metrics.count('ContextualSearch.CurrentTabChipClick.NewTabPage', 0));
    // Assert context added method was current tab chip.
    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.ContextAdded.ContextAddedMethod.NewTabPage',
            /* CURRENT_TAB_CHIP */ 3));
  });

  test('has correct text and title', () => {
    const button = getButton();
    const buttonText = button.querySelector('.current-tab-button-text');
    assertTrue(!!buttonText);
    assertEquals('Ask about this page', buttonText.textContent.trim());
    assertEquals('Tab 1 - tab1.com', button.title);
  });

  test('becomes visible when tabs are added', async () => {
    currentTabChip.currentTab = undefined;
    await microtasksFinished();
    assertEquals(null, $$(currentTabChip, '#currentTabButton'));
    currentTabChip.currentTab = MOCK_TAB_INFO;
    await microtasksFinished();
    assertTrue(getButton() !== null);
  });

  test('has correct aria-label', () => {
    const button = getButton();
    assertEquals(
        'Ask Google about this page: Tab 1 - tab1.com', button.ariaLabel);
  });
});
