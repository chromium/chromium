// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {ActionChipsType} from 'chrome://new-tab-page/lazy_load.js';
import type {ActionChipsElement} from 'chrome://new-tab-page/lazy_load.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('NewTabPageActionChipsTest', () => {
  let chips: ActionChipsElement;
  let metrics: MetricsTracker;

  setup(() => {
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
