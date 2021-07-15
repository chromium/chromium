// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/accelerator_row.js';
import {ModifierKeys} from 'chrome://shortcut-customization/accelerator_view.js';

import {assertEquals} from '../../chai_assert.js';

export function acceleratorRowTest() {
  /** @type {?AcceleratorRowElement} */
  let rowElement = null;

  setup(() => {
    rowElement = /** @type {!AcceleratorRowElement} */ (
        document.createElement('accelerator-row'));
    document.body.appendChild(rowElement);
  });

  teardown(() => {
    rowElement.remove();
    rowElement = null;
  });

  test('LoadsBasicRow', async () => {
    // TODO(jimmyxgong): Update the type of the test accelerator with the mojom
    // version.
    const accelerators = [
      {
        modifiers: ModifierKeys.SHIFT | ModifierKeys.CONTROL,
        key: 'g',
        rawKey: 0x0
      },
      {modifiers: ModifierKeys.CONTROL, key: 'c', rawKey: 0x0}
    ];
    const description = 'test shortcut';

    rowElement.accelerators = accelerators;
    rowElement.description = description;
    await flush();
    const acceleratorElements =
        rowElement.shadowRoot.querySelectorAll('accelerator-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description,
        rowElement.shadowRoot.querySelector('#descriptionText')
            .textContent.trim());

    const keys1 =
        acceleratorElements[0].shadowRoot.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);
    assertEquals(
        'shift', keys1[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'ctrl', keys1[1].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'g', keys1[2].shadowRoot.querySelector('#key').textContent.trim());

    const keys2 =
        acceleratorElements[1].shadowRoot.querySelectorAll('input-key');
    // CONTROL + c
    assertEquals(2, keys2.length);
    assertEquals(
        'ctrl', keys2[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'c', keys2[1].shadowRoot.querySelector('#key').textContent.trim());
  });
}