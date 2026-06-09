// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_labels.js';

import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import type {PowerBookmarksLabelsElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_labels.js';
import type {BookmarkProductInfo} from 'chrome://resources/cr_components/commerce/shared.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createMockTrackedProduct(): BookmarkProductInfo {
  return {
    bookmarkId: BigInt(0),
    info: {
      title: '',
      clusterTitle: '',
      domain: '',
      imageUrl: '',
      productUrl: '',
      currentPrice: '',
      previousPrice: '',
      clusterId: BigInt(0),
      categoryLabels: [],
      priceSummary: '',
    },
  };
}

suite('SidePanelPowerBookmarksLabelsTest', () => {
  let element: PowerBookmarksLabelsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      priceTrackingLabel: 'Tracked products',
    });

    element = document.createElement('power-bookmarks-labels');
    document.body.appendChild(element);
  });

  test('NoProductsNoLabels', async () => {
    element.trackedProductInfos = {};
    await microtasksFinished();
    assertEquals(0, element.labels.length);
  });

  test('TrackedProductsLabel', async () => {
    const trackedProductInfos = {
      '1000': createMockTrackedProduct(),
    };
    element.trackedProductInfos = trackedProductInfos;
    await microtasksFinished();
    assertEquals(1, element.labels.length);
    assertEquals('Tracked products', element.labels[0]!.label);
    assertEquals('bookmarks:price-tracking', element.labels[0]!.icon);
    assertFalse(element.labels[0]!.active);
  });

  test('UpdatesAndPersistsActiveState', async () => {
    element.trackedProductInfos = {
      '1000': createMockTrackedProduct(),
      '2000': createMockTrackedProduct(),
    };
    await microtasksFinished();
    assertEquals(1, element.labels.length);
    assertFalse(element.labels[0]!.active);

    let labelsChangedPromise = eventToPromise('labels-changed', element);
    const labelChip = element.shadowRoot.querySelector('cr-chip')!;
    labelChip.click();
    assertTrue(element.labels[0]!.active);
    await labelsChangedPromise;

    // Changing tracked products should update labels but not active states.
    labelsChangedPromise = eventToPromise('labels-changed', element);
    element.trackedProductInfos = {'1000': createMockTrackedProduct()};
    await microtasksFinished();
    await labelsChangedPromise;
    assertTrue(element.labels[0]!.active);
  });

  test('UpdatesIcon', async () => {
    element.trackedProductInfos = {
      '1000': createMockTrackedProduct(),
    };
    await microtasksFinished();

    const labelChipIcon =
        element.shadowRoot.querySelector<CrIconElement>('cr-chip cr-icon')!;
    assertEquals('bookmarks:price-tracking', labelChipIcon.icon);

    element.shadowRoot.querySelector('cr-chip')!.click();
    await microtasksFinished();
    assertEquals('bookmarks:check', labelChipIcon.icon);
  });

  test('UpdatesDisabledState', async () => {
    element.trackedProductInfos = {
      '1000': createMockTrackedProduct(),
    };
    await microtasksFinished();

    const labelChip = element.shadowRoot.querySelector('cr-chip')!;
    assertFalse(labelChip.disabled);

    element.disabled = true;
    await microtasksFinished();
    assertTrue(labelChip.disabled);
  });
});
