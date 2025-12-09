
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_lens_search.js';

import type {ComposeboxLensSearchElement} from 'chrome://resources/cr_components/composebox/composebox_lens_search.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';


suite('ComposeboxLensSearch', () => {
  const LABEL_TEXT = 'Lens Search';
  let lensSearch: ComposeboxLensSearchElement;

  suiteSetup(() => {
    loadTimeData.resetForTesting({lensSearchLabel: LABEL_TEXT});
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSearch = document.createElement('cr-composebox-lens-search');
    document.body.appendChild(lensSearch);
    await microtasksFinished();
    assertTrue(!!lensSearch);
  });

  test('renders icon and label correctly', () => {
    const button = $$<CrButtonElement>(lensSearch, 'cr-button');
    assertTrue(!!button);
    assertEquals(LABEL_TEXT, button.getAttribute('aria-label'));
    assertEquals(LABEL_TEXT, button.getAttribute('title'));

    const content = $$(lensSearch, '#content');
    assertTrue(!!content);
    assertEquals(LABEL_TEXT, content.textContent.trim());

    const icon = $$<CrIconElement>(lensSearch, 'cr-icon');
    assertTrue(!!icon);
    assertTrue(isVisible(icon));
    assertEquals('composebox:google-lens-2', icon.icon);
  });

  test('dispatches click event', async () => {
    const whenFired = eventToPromise('lens-search-click', lensSearch);
    const button = $$(lensSearch, 'cr-button');
    assertTrue(!!button);

    button.click();
    return whenFired;
  });
});
