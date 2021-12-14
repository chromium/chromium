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
}
