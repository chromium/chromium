// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_edit_dialog.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditDialogElement} from 'chrome://shortcut-customization/js/accelerator_edit_dialog.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/js/accelerator_edit_view.js';
import {AcceleratorInfo, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorEditDialogTest', function() {
  let viewElement: AcceleratorEditDialogElement|null = null;

  setup(() => {
    viewElement = document.createElement('accelerator-edit-dialog');
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    viewElement!.remove();
    viewElement = null;
  });

  test('LoadsBasicDialog', async () => {
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];

    const description = 'test shortcut';

    viewElement!.acceleratorInfos = accelerators;
    viewElement!.description = description;
    await flush();
    const dialog =
        viewElement!.shadowRoot!.querySelector('cr-dialog') as CrDialogElement;
    assertTrue(dialog.open);
    const acceleratorElements =
        dialog.querySelectorAll('accelerator-edit-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description,
        dialog!.querySelector('#dialogTitle')!.textContent!.trim());

    const accelView1 =
        acceleratorElements[0]!.shadowRoot!.querySelector('accelerator-view');
    const keys1 = accelView1!.shadowRoot!.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);
    assertEquals(
        'shift',
        keys1[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'ctrl',
        keys1[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys1[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    const accelView2 =
        acceleratorElements[1]!.shadowRoot!.querySelector('accelerator-view');
    const keys2 = accelView2!.shadowRoot!.querySelectorAll('input-key');
    // CONTROL + c
    assertEquals(2, keys2.length);
    assertEquals(
        'ctrl',
        keys2[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'c', keys2[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    // Clicking on "Done" button will close the dialog.
    const button = dialog!.querySelector('#doneButton') as CrButtonElement;
    button.click();
    assertFalse(dialog.open);
  });

  test('AddShortcut', async () => {
    const acceleratorInfo1: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2: AcceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const acceleratorInfos = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    viewElement!.acceleratorInfos = acceleratorInfos;
    viewElement!.description = description;
    await flush();
    const dialog = viewElement!.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // The "Add Shortcut" button should be visible and the pending accelerator
    // should not be visible.
    const buttonContainer =
        dialog!.querySelector('#addAcceleratorContainer') as HTMLDivElement;
    assertTrue(!!buttonContainer);
    assertFalse(buttonContainer!.hidden);
    let pendingAccelerator: AcceleratorEditViewElement|null =
        dialog!.querySelector('#pendingAccelerator');
    assertFalse(!!pendingAccelerator);

    // Clicking on the "Add Shortcut" button should hide the button and show
    // the pending shortcut.
    const addButton =
        dialog!.querySelector('#addAcceleratorButton') as CrButtonElement;
    addButton!.click();
    await flush();
    assertTrue(buttonContainer!.hidden);
    // Expected the dialog's "done" button to be disabled when adding a new
    // accelerator.
    const doneButton = dialog!.querySelector('#doneButton') as CrButtonElement;
    assertTrue(doneButton!.disabled);
    const restoreButton =
        dialog!.querySelector('#restoreDefault') as CrButtonElement;
    assertTrue(restoreButton!.hidden);

    // Re-query the stamped element.
    pendingAccelerator = dialog!.querySelector('#pendingAccelerator');
    assertTrue(!!pendingAccelerator);

    // Click on the cancel button, expect the "Add Shortcut" button to be
    // visible and the pending accelerator to be hidden.
    const cancelButton = pendingAccelerator!.shadowRoot!.querySelector(
                             '#cancelButton') as CrButtonElement;
    cancelButton.click();
    await flush();

    // "done" button should now be enabled.
    assertFalse(doneButton!.disabled);
    assertFalse(restoreButton!.hidden);

    assertFalse(buttonContainer!.hidden);
    // Re-query the stamped element.
    pendingAccelerator = dialog!.querySelector('#pendingAccelerator');
    assertFalse(!!pendingAccelerator);
  });
});
