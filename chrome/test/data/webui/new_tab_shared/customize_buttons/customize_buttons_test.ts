// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CustomizeButtonsElement} from 'chrome://newtab-shared/customize_buttons/customize_buttons.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('CustomizeButtons', () => {
  let element: CustomizeButtonsElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('ntp-customize-buttons');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('shows shadow according to property', async () => {
    const button = element.$.customizeButton;
    assertTrue(hasStyle(button, 'box-shadow', 'none'));

    element.showShadow = true;
    await microtasksFinished();
    assertFalse(hasStyle(button, 'box-shadow', 'none'));

    element.showBackgroundImage = true;
    await microtasksFinished();
    assertTrue(hasStyle(button, 'box-shadow', 'none'));
  });
});
