// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/recent_tab_chip.js';

import type {RecentTabChipElement} from 'chrome://resources/cr_components/composebox/recent_tab_chip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('RecentTabChipTest', function() {
  let recentTabChip: RecentTabChipElement;
  let metrics: MetricsTracker;

  const MOCK_TAB_INFO: TabInfo = {
    tabId: 1,
    title: 'Tab 1',
    url: {url: 'https://tab1.com'},
    showInCurrentTabChip: false,
    showInPreviousTabChip: true,
    lastActive: {internalValue: 1n},
  };

  setup(async () => {
    loadTimeData.overrideValues({
      addTabUploadDelayOnRecentTabChipClick: false,
    });
    metrics = fakeMetricsPrivate();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    recentTabChip = document.createElement('composebox-recent-tab-chip');
    document.body.appendChild(recentTabChip);
    recentTabChip.recentTab = MOCK_TAB_INFO;
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
    recentTabChip.recentTab = undefined;
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
    assertFalse(event.detail.delayUpload);
  });

  test('delayUploads is true when flag is enabled', async () => {
    loadTimeData.overrideValues({
      addTabUploadDelayOnRecentTabChipClick: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    recentTabChip = document.createElement('composebox-recent-tab-chip');
    document.body.appendChild(recentTabChip);
    recentTabChip.recentTab = MOCK_TAB_INFO;
    await microtasksFinished();

    const eventPromise = eventToPromise('add-tab-context', recentTabChip);
    const button = getButton();
    button.click();

    const event = await eventPromise;
    assertTrue(event !== null);
    assertEquals(MOCK_TAB_INFO.tabId, event.detail.id);
    assertEquals(MOCK_TAB_INFO.title, event.detail.title);
    assertEquals(MOCK_TAB_INFO.url, event.detail.url);
    assertTrue(event.detail.delayUpload);
    // Assert context added method was context menu.
    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.ContextAdded.ContextAddedMethod.NewTabPage',
            /* RECENT_TAB_CHIP */ 3));
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
    recentTabChip.recentTab = undefined;
    await microtasksFinished();
    assertEquals(null, $$(recentTabChip, '#recentTabButton'));

    recentTabChip.recentTab = MOCK_TAB_INFO;
    await microtasksFinished();
    assertTrue(getButton() !== null);
  });

  test('has correct aria-label', () => {
    const button = getButton();
    assertEquals('Ask Google about previous tab: Tab 1', button.ariaLabel);
  });
});
