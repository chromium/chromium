// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/memory_saver_card.js';

import {MemorySaverCardElement} from 'chrome://performance-side-panel.top-chrome/memory_saver_card.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('memory saver card', () => {
  let memorySaverCard: MemorySaverCardElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    memorySaverCard = document.createElement('memory-saver-card');

    document.body.appendChild(memorySaverCard);
  });

  // TODO: Add more meaningful tests when the card has actual content
  test('heading exists', () => {
    const heading = memorySaverCard.shadowRoot!.querySelector('sp-heading');
    assertTrue(!!heading);
    assertEquals(heading.innerText, 'Memory Saver');
  });
});
