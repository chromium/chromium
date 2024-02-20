// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {ApnSelectionDialog} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('<apn-selection-dialog>', () => {
  let apnSelectionDialog: ApnSelectionDialog;

  suiteSetup(() => {
    apnSelectionDialog = document.createElement('apn-selection-dialog');
    document.body.appendChild(apnSelectionDialog);
    return waitAfterNextRender(apnSelectionDialog);
  });

  teardown(() => {
    apnSelectionDialog.remove();
  });

  test('Element contains dialog', async () => {
    const dialog = apnSelectionDialog.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
  });
});
