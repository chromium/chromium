// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('HotspotSummaryItemTest', function() {
  /** @type {HotspotSummaryItemElement} */
  let hotspotSummaryItem = null;

  setup(function() {
    PolymerTest.clearBody();
    hotspotSummaryItem = document.createElement('hotspot-summary-item');
    document.body.appendChild(hotspotSummaryItem);
    flush();
  });

  teardown(function() {
    hotspotSummaryItem.remove();
    hotspotSummaryItem = null;
    Router.getInstance().resetRouteForTesting();
  });

  test('clicking on subpage arrow routes to hotspot subpage', async function() {
    const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
        '#hotspotSummaryItemRowArrowIcon');
    assertTrue(!!subpageArrow);
    subpageArrow.click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.HOTSPOT_DETAIL);
  });

  test(
      'clicking on hotspot summary row routes to hotspot subpage',
      async function() {
        const subpageArrow = hotspotSummaryItem.shadowRoot.querySelector(
            '#hotspotSummaryItemRow');
        assertTrue(!!subpageArrow);
        subpageArrow.click();
        assertEquals(
            Router.getInstance().getCurrentRoute(), routes.HOTSPOT_DETAIL);
      });
});