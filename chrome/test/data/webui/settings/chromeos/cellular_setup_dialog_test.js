// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CellularSetupDialog', function() {
  test('Dialog opened on attach', function(done) {
    /** @type {!OsSettingsCellularSetupDialog} */
    const cellularSetupDialog =
        document.createElement('os-settings-cellular-setup-dialog');

    // Verify the dialog is opened.
    cellularSetupDialog.addEventListener('cr-dialog-open', function(e) {
      done();
    });

    // Attach the element to the DOM, which opens the dialog.
    document.body.appendChild(cellularSetupDialog);
    Polymer.dom.flush();

    assertTrue(cellularSetupDialog.$.dialog.open);
  });
});
