// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {ActionChipsElement} from 'chrome://new-tab-page/lazy_load.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('NewTabPageActionChipsTest', () => {
  let chips: ActionChipsElement;

  setup(() => {
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
  });
});
