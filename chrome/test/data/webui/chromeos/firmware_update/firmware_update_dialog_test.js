// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogState, FirmwareUpdateDialogElement} from 'chrome://accessory-update/firmware_update_dialog.js';
import {FirmwareUpdate, UpdatePriority} from 'chrome://accessory-update/firmware_update_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {isVisible} from '../../test_util.js';

export function firmwareUpdateDialogTest() {
  /** @type {?FirmwareUpdateDialogElement} */
  let updateDialog = null;

  setup(() => {
    updateDialog = /** @type {!FirmwareUpdateDialogElement} */ (
        document.createElement('firmware-update-dialog'));
    document.body.appendChild(updateDialog);
  });

  teardown(() => {
    updateDialog.remove();
    updateDialog = null;
  });

  test('LoadDevicePrepDialog', async () => {
    /** @type {!FirmwareUpdate} */
    const fakeFirmwareUpdate = {
      deviceId: '1',
      deviceName: 'Logitech keyboard',
      version: '2.1.12',
      description:
          'Update firmware for Logitech keyboard to improve performance',
      priority: UpdatePriority.kLow,
      updateModeInstructions: 'Do a backflip before updating.',
      screenshotUrl: '',
    };

    updateDialog.update = fakeFirmwareUpdate;
    updateDialog.dialogState = DialogState.DEVICE_PREP;
    await flush();
    const dialog = updateDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(dialog.open);

    assertEquals(
        fakeFirmwareUpdate.updateModeInstructions,
        dialog.querySelector('#updateInstructions').textContent.trim());

    // Clicking on "Cancel" button will close the dialog.
    const button = dialog.querySelector('#cancelButton');
    button.click();
    await flush();
    assertFalse(isVisible(dialog));
  });
}
