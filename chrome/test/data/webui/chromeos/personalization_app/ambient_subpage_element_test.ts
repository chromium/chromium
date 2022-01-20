// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/trusted/ambient/toggle_row.js';

import {AmbientSubpage} from 'chrome://personalization/trusted/ambient/ambient_subpage_element.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

export function AmbientSubpageTest() {
  let ambientSubpageElement: AmbientSubpage|null;

  setup(function() {});

  teardown(async () => {
    await teardownElement(ambientSubpageElement);
    ambientSubpageElement = null;
  });

  test('displays content', async () => {
    ambientSubpageElement = initElement(AmbientSubpage);
    assertEquals(
        'Ambient',
        ambientSubpageElement.shadowRoot!.querySelector('h2')!.innerText);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow);
    const toggleButton =
        toggleRow!.shadowRoot!.querySelector('cr-toggle') as CrToggleElement;
    assertTrue(!!toggleButton);
    assertFalse(toggleButton!.checked);
  });
}
