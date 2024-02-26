// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_selection_dialog.js';

import {ApnSelectionDialog} from 'chrome://resources/ash/common/network/apn_selection_dialog.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnSelectionDialog', () => {
  /** @type {ApnSelectionDialog} */
  let apnSelectionDialog = null;

  setup(function() {
    apnSelectionDialog = document.createElement('apn-selection-dialog');
    document.body.appendChild(apnSelectionDialog);
    return waitAfterNextRender(apnSelectionDialog);
  });

  teardown(() => {
    apnSelectionDialog.remove();
  });

  test('Element contains dialog', async () => {
    const dialog = apnSelectionDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
  });
});
