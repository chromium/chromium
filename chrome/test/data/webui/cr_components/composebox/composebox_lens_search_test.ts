
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_lens_search.js';

import type {ComposeboxLensSearchElement} from 'chrome://resources/cr_components/composebox/composebox_lens_search.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';


suite('ComposeboxLensSearch', () => {
  const HINT_TEXT = 'Lens Search';
  let lensSearch: ComposeboxLensSearchElement;

  suiteSetup(() => {
    loadTimeData.resetForTesting({lensSearchHint: HINT_TEXT});
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSearch = document.createElement('cr-composebox-lens-search');
    document.body.appendChild(lensSearch);
    await microtasksFinished();
    assertTrue(!!lensSearch);
  });

  test('renders label correctly', () => {
    const button = $$<CrButtonElement>(lensSearch, 'cr-button');
    assertTrue(!!button);
    assertEquals(HINT_TEXT, button.getAttribute('aria-label'));
    assertEquals(HINT_TEXT, button.getAttribute('title'));

    const content = $$(lensSearch, '#content');
    assertTrue(!!content);
    assertEquals(HINT_TEXT, content.textContent.trim());

    const icon = $$(lensSearch, 'cr-icon');
    assertFalse(!!icon);
  });

  test('dispatches click event', async () => {
    const whenFired = eventToPromise('lens-search-click', lensSearch);
    const button = $$(lensSearch, 'cr-button');
    assertTrue(!!button);

    button.click();
    return whenFired;
  });
});
