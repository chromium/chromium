// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PlaceholderTextCycler} from 'chrome://resources/cr_components/searchbox/placeholder_text_cycler.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {waitForAttributeChange} from './searchbox_test_utils.js';

suite('PlaceholderTextCyclerTest', () => {
  let testInputElement: HTMLInputElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testInputElement = document.createElement('input');
    testInputElement.type = 'text';
    document.body.appendChild(testInputElement);
  });

  test('start and stop cycling input placeholder', async () => {
    const sampleTransitionPlaceholder = 'Make a plan';
    const placeholderTextCycler: PlaceholderTextCycler =
        new PlaceholderTextCycler(
            testInputElement, ['Ask Google', sampleTransitionPlaceholder], 50,
            25);
    placeholderTextCycler.start();
    const text =
        await waitForAttributeChange(testInputElement, 'placeholder', '');
    assertEquals(sampleTransitionPlaceholder, text);

    placeholderTextCycler.stop();
  });
});
