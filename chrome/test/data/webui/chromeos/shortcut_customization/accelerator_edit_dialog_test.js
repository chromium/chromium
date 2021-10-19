// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditDialogElement} from 'chrome://shortcut-customization/accelerator_edit_dialog.js';
import {AcceleratorInfo, AcceleratorKeys, AcceleratorState, AcceleratorType, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {CreateUserAccelerator} from './shortcut_customization_test_util.js';

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
    /** @type {!AcceleratorInfo} */
    const acceleratorInfo1 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');

    /** @type {!AcceleratorInfo} */
    const acceleratorInfo2 = CreateUserAccelerator(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*key_display=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];

    const description = 'test shortcut';

    viewElement.acceleratorInfos = accelerators;
    viewElement.description = description;
    await flush();
    const dialog = viewElement.shadowRoot.querySelector('cr-dialog');
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description, dialog.querySelector('#dialogTitle').textContent.trim());

    const accelView1 =
        acceleratorElements[0].shadowRoot.querySelector('accelerator-view');
    const keys1 = accelView1.shadowRoot.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);
    assertEquals(
        'shift', keys1[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'ctrl', keys1[1].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'g', keys1[2].shadowRoot.querySelector('#key').textContent.trim());

    const accelView2 =
        acceleratorElements[1].shadowRoot.querySelector('accelerator-view');
    const keys2 = accelView2.shadowRoot.querySelectorAll('input-key');
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

  test('AddShortcut', async () => {
    /** @type {!AcceleratorInfo} */
    const acceleratorInfo1 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');

    /** @type {!AcceleratorInfo} */
    const acceleratorInfo2 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*key_display=*/ 'c');

    const acceleratorInfos = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    viewElement.acceleratorInfos = acceleratorInfos;
    viewElement.description = description;
    await flush();
    const dialog = viewElement.shadowRoot.querySelector('cr-dialog');
    assertTrue(dialog.open);

    // The "Add Shortcut" button should be visible and the pending accelerator
    // should not be visible.
    const buttonContainer = dialog.querySelector('#addAcceleratorContainer');
    assertFalse(buttonContainer.hidden);
    let pendingAccelerator = dialog.querySelector('#pendingAccelerator');
    assertFalse(!!pendingAccelerator);

    // Clicking on the "Add Shortcut" button should hide the button and show
    // the pending shortcut.
    const addButton = dialog.querySelector('#addAcceleratorButton');
    addButton.click();
    await flush();
    assertTrue(buttonContainer.hidden);
    // Expected the dialog's "done" button to be disabled when adding a new
    // accelerator.
    const doneButton = dialog.querySelector('#doneButton');
    assertTrue(doneButton.disabled);
    const restoreButton = dialog.querySelector('#restoreDefault');
    assertTrue(restoreButton.hidden);

    // Re-query the stamped element.
    pendingAccelerator = dialog.querySelector('#pendingAccelerator');
    assertTrue(!!pendingAccelerator);

    // Click on the cancel button, expect the "Add Shortcut" button to be
    // visible and the pending accelerator to be hidden.
    pendingAccelerator.shadowRoot.querySelector('#cancelButton').click();
    await flush();

    // "done" button should now be enabled.
    assertFalse(doneButton.disabled);
    assertFalse(restoreButton.hidden);

    assertFalse(buttonContainer.hidden);
    // Re-query the stamped element.
    pendingAccelerator = dialog.querySelector('#pendingAccelerator');
    assertFalse(!!pendingAccelerator);
  });
}