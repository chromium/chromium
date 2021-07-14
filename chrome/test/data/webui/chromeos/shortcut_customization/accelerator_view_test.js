// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorViewElement, ModifierKeys} from 'chrome://shortcut-customization/accelerator_view.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

export function acceleratorViewTest() {
  /** @type {?AcceleratorViewElement} */
  let viewElement = null;

  setup(() => {
    viewElement = /** @type {!AcceleratorViewElement} */ (
        document.createElement('accelerator-view'));
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    viewElement.remove();
    viewElement = null;
  });

  test('LoadsBasicAccelerator', async () => {
    // TODO(jimmyxgong): Update the type of the test accelerator with the mojom
    // version.
    const accelerator = {
      modifiers: ModifierKeys.SHIFT | ModifierKeys.CONTROL,
      key: 'g',
      rawKey: 0x0,
    };

    viewElement.accelerator = accelerator;
    await flush();
    const keys = viewElement.shadowRoot.querySelectorAll('input-key');
    // Three keys: shift, control, g
    assertEquals(3, keys.length);

    assertEquals(
        'shift', keys[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'ctrl', keys[1].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'g', keys[2].shadowRoot.querySelector('#key').textContent.trim());
  });
}