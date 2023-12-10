// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/browser_health_card.js';

import {BrowserHealthCardElement} from 'chrome://performance-side-panel.top-chrome/browser_health_card.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Browser health card', () => {
  let browserHealthCard: BrowserHealthCardElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserHealthCard = document.createElement('browser-health-card');

    document.body.appendChild(browserHealthCard);
  });

  test('heading exists', () => {
    const heading = browserHealthCard.shadowRoot!.querySelector('sp-heading');
    assertTrue(!!heading);
    assertEquals(heading.innerText.trim(), 'Browser Health');
  });

  test('hides when features are disabled', () => {
    loadTimeData.overrideValues({
      isPerformanceCPUInterventionEnabled: false,
      isPerformanceMemoryInterventionEnabled: false,
    });
    assertTrue(browserHealthCard.hidden);
  });
});
