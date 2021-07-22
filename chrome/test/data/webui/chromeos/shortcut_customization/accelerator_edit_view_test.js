// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorEditViewElement} from 'chrome://shortcut-customization/accelerator_edit_view.js';
import {ModifierKeys} from 'chrome://shortcut-customization/accelerator_view.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

export function acceleratorEditViewTest() {
  /** @type {?AcceleratorEditViewElement} */
  let editViewElement = null;

  setup(() => {
    editViewElement = /** @type {!AcceleratorEditViewElement} */ (
        document.createElement('accelerator-edit-view'));
    document.body.appendChild(editViewElement);
  });

  teardown(() => {
    editViewElement.remove();
    editViewElement = null;
  });

  test('LoadsBasicEditView', async () => {
    // TODO(jimmyxgong): Update the type of the test accelerator with the mojom
    // version.
    const accelerator = {
      modifiers: ModifierKeys.SHIFT | ModifierKeys.CONTROL,
      key: 'g',
      rawKey: 0x0
    };

    editViewElement.accelerator = accelerator;
    await flush();

    // Check that the edit buttons are visible.
    assertFalse(
        editViewElement.shadowRoot.querySelector('#editButtonsContainer')
            .hidden);
    assertTrue(
        editViewElement.shadowRoot.querySelector('#cancelButtonContainer')
            .hidden);

    // Click on the edit button.
    editViewElement.shadowRoot.querySelector('#editButton').click();

    // Only the Cancel button should now be visible.
    assertTrue(editViewElement.shadowRoot.querySelector('#editButtonsContainer')
                   .hidden);
    assertFalse(
        editViewElement.shadowRoot.querySelector('#cancelButtonContainer')
            .hidden);

    // Click on the Cancel button and expect the edit buttons to be available.
    editViewElement.shadowRoot.querySelector('#cancelButton').click();
    assertFalse(
        editViewElement.shadowRoot.querySelector('#editButtonsContainer')
            .hidden);
    assertTrue(
        editViewElement.shadowRoot.querySelector('#cancelButtonContainer')
            .hidden);
  });
}