// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FirmwareUpdateDialogElement} from 'chrome://accessory-update/firmware_update_dialog.js';

import {assertTrue} from '../../chai_assert.js';

export function firmwareUpdateDialogTest() {
  /** @type {?FirmwareUpdateDialogElement} */
  let dialog = null;

  setup(() => {
    dialog = /** @type {!FirmwareUpdateDialogElement} */ (
        document.createElement('firmware-update-dialog'));
    document.body.appendChild(dialog);
  });

  teardown(() => {
    dialog.remove();
    dialog = null;
  });

  test('DialogInitialized', () => {
    // TODO(michaelcheco): Remove this stub test once the page has more
    // capabilities to test.
    assertTrue(!!dialog.shadowRoot.querySelector('#firmwareUpdateDialog'));
  });
}
