// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {OsSettingsCellularSetupDialogElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<os-settings-cellular-setup-dialog>', () => {
  test('Dialog opened on attach', (done) => {
    const cellularSetupDialog: OsSettingsCellularSetupDialogElement =
        document.createElement('os-settings-cellular-setup-dialog');

    // Verify the dialog is opened.
    cellularSetupDialog.addEventListener('cr-dialog-open', () => {
      done();
    });

    // Attach the element to the DOM, which opens the dialog.
    document.body.appendChild(cellularSetupDialog);
    flush();

    assertTrue(cellularSetupDialog.$.dialog.open);
  });
});
