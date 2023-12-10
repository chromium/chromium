// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/battery_saver_card.js';

import {BatterySaverCardElement} from 'chrome://performance-side-panel.top-chrome/battery_saver_card.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Battery saver card', () => {
  let batterySaverCard: BatterySaverCardElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    batterySaverCard = document.createElement('battery-saver-card');

    document.body.appendChild(batterySaverCard);
  });

  // TODO: Add more meaningful tests when the card has actual content
  test('heading exists', () => {
    const heading = batterySaverCard.shadowRoot!.querySelector('sp-heading');
    assertTrue(!!heading);
    assertEquals(heading.innerText, 'Battery Saver');
  });
});
