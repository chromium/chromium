// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {assertTrue, assertFalse} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('SwitchAccessActionAssignmentDialogTest', function() {
  /** @type {!SettingsSwitchAccessActionAssignmentDialog} */
  let dialog;

  setup(function() {
    dialog = document.createElement('settings-switch-access-action-assignment-dialog');
    dialog.action = 'select';
    document.body.appendChild(dialog);
    Polymer.dom.flush();
  });

  test('Exit button closes dialog', function() {
    assertTrue(dialog.$.switchAccessActionAssignmentDialog.open);

    const exitBtn = dialog.$.exit;
    assertTrue(!!exitBtn);

    exitBtn.click();
    assertFalse(dialog.$.switchAccessActionAssignmentDialog.open);
  });
});
