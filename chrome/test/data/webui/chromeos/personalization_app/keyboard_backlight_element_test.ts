// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {KeyboardBacklight} from 'chrome://personalization/trusted/personalization_app.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('KeyboardBacklightTest', function() {
  let keyboardBacklightElement: KeyboardBacklight|null;

  setup(() => {
    baseSetup();
  });

  teardown(async () => {
    await teardownElement(keyboardBacklightElement);
    keyboardBacklightElement = null;
  });


  test('displays content', async () => {
    keyboardBacklightElement = initElement(KeyboardBacklight);
    const labelContainer = keyboardBacklightElement.shadowRoot!.getElementById(
        'keyboardBacklightLabel');
    assertTrue(!!labelContainer);
    const text = labelContainer.querySelector('p');
    assertTrue(!!text);
    assertEquals(
        keyboardBacklightElement.i18n('keyboardBacklightTitle'),
        text.textContent);

    const selectorContainer =
        keyboardBacklightElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers =
        selectorContainer.querySelectorAll('.color-container');
    assertEquals(9, colorContainers!.length);
  });
});
