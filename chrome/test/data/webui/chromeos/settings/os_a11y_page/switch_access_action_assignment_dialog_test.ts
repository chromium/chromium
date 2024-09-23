// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsSwitchAccessActionAssignmentDialogElement, SwitchAccessCommand} from 'chrome://os-settings/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<settings-switch-access-action-assignment-dialog>', () => {
  let dialog: SettingsSwitchAccessActionAssignmentDialogElement;

  setup(() => {
    dialog = document.createElement(
        'settings-switch-access-action-assignment-dialog');
    dialog.action = SwitchAccessCommand.SELECT;
    document.body.appendChild(dialog);
    flush();
  });

  test('Exit button closes dialog', () => {
    assertTrue(dialog.$.switchAccessActionAssignmentDialog.open);

    const exitBtn = dialog.$.switchAccessActionAssignmentDialog.$.close;
    assertTrue(!!exitBtn);

    exitBtn.click();
    assertFalse(dialog.$.switchAccessActionAssignmentDialog.open);
  });
});
