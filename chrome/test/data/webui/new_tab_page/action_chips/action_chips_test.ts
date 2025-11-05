// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {TabInfo} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import {ActionChipsHandlerRemote} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import {ActionChipsApiProxyImpl, ActionChipsType} from 'chrome://new-tab-page/lazy_load.js';
import type {ActionChipsElement} from 'chrome://new-tab-page/lazy_load.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

suite('NewTabPageActionChipsTest', () => {
  let chips: ActionChipsElement;
  let metrics: MetricsTracker;

  setup(() => {
    const handler = installMock(
        ActionChipsHandlerRemote,
        mock => ActionChipsApiProxyImpl.setInstance({getHandler: () => mock}));
    handler.setResultFor('getMostRecentTab', Promise.resolve({tab: null}));
    metrics = fakeMetricsPrivate();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chips = document.createElement('ntp-action-chips');
    document.body.append(chips);
  });

  test('nano banana chip triggers chip click event', async () => {
    // Setup.
    const nanoBananaChip =
        chips.shadowRoot.querySelector<CrButtonElement>('#nano-banana');
    assertTrue(!!nanoBananaChip);
    const whenActionChipClicked =
        eventToPromise('action-chip-click', document.body);
    nanoBananaChip.click();

    // Assert.
    await whenActionChipClicked;

    assertEquals(2, metrics.count('NewTabPage.ActionChips.Shown'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ActionChips.Shown', ActionChipsType.CREATE_IMAGE));

    assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ActionChips.Click', ActionChipsType.CREATE_IMAGE));
  });
  test('deep search chip triggers chip click event', async () => {
    // Setup.
    const deepSearchChip =
        chips.shadowRoot.querySelector<CrButtonElement>('#deep-search');
    assertTrue(!!deepSearchChip);
    const whenActionChipClicked =
        eventToPromise('action-chip-click', document.body);
    deepSearchChip.click();

    // Assert.
    await whenActionChipClicked;

    assertEquals(2, metrics.count('NewTabPage.ActionChips.Shown'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ActionChips.Shown', ActionChipsType.DEEP_SEARCH));

    assertEquals(1, metrics.count('NewTabPage.ActionChips.Click'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.ActionChips.Click', ActionChipsType.DEEP_SEARCH));
  });
});

// Suite for "Most Recent Tab" functionality
suite('ActionChipsMostRecentTab', () => {
  let handler: TestMock<ActionChipsHandlerRemote>;
  let chips: ActionChipsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        ActionChipsHandlerRemote,
        mock => ActionChipsApiProxyImpl.setInstance({getHandler: () => mock}));
  });

  async function initializeChips(tab: TabInfo|null) {
    handler.setResultFor('getMostRecentTab', Promise.resolve({tab}));
    chips = document.createElement('ntp-action-chips');
    document.body.append(chips);
    await handler.whenCalled('getMostRecentTab');
  }

  test('Handler is called on load', async () => {
    await initializeChips(null);

    assertTrue(!!chips);
    assertEquals(1, handler.getCallCount('getMostRecentTab'));
  });

  test('No tab info when no recent tab is returned', async () => {
    await initializeChips(null);

    assertEquals(1, handler.getCallCount('getMostRecentTab'));
    assertEquals(null, chips.mostRecentTab);
  });

  test('Most recent tab info is found on return', async () => {
    const fakeTab: TabInfo = {
      title: 'Test Title',
      url: {url: 'https://example.com/test'},
      lastActiveTime: {internalValue: BigInt(12345)},
    };
    await initializeChips(fakeTab);

    assertEquals(1, handler.getCallCount('getMostRecentTab'));
    assertDeepEquals(fakeTab, chips.mostRecentTab);
  });
});
