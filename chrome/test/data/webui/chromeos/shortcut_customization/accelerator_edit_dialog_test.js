// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditDialogElement} from 'chrome://shortcut-customization/accelerator_edit_dialog.js';
import {ModifierKeys} from 'chrome://shortcut-customization/accelerator_view.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

export function acceleratorEditDialogTest() {
  /** @type {?AcceleratorEditDialogElement} */
  let viewElement = null;

  setup(() => {
    viewElement = /** @type {!AcceleratorEditDialogElement} */ (
        document.createElement('accelerator-edit-dialog'));
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    viewElement.remove();
    viewElement = null;
  });

  test('LoadsBasicDialog', async () => {
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

    viewElement.accelerators = accelerators;
    viewElement.description = description;
    await flush();
    const dialog = viewElement.shadowRoot.querySelector('cr-dialog');
    assertTrue(dialog.open);
    const acceleratorElements = dialog.querySelectorAll('accelerator-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description, dialog.querySelector('#dialogTitle').textContent.trim());

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

    // Clicking on "Done" button will close the dialog.
    const button = dialog.querySelector('#doneButton');
    button.click();
    assertFalse(dialog.open);
  });
}