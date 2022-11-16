// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_detail_dialog.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertTrue} from '../../../chai_assert.js';

suite('ApnDetailDialog', function() {
  /** @type {ApnDetalDialog} */
  let apnDetailDialog = null;

  setup(function() {
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    apnDetailDialog = document.createElement('apn-detail-dialog');
    document.body.appendChild(apnDetailDialog);

    return flushTasks();
  });

  teardown(function() {
    return flushTasks().then(() => {
      apnDetailDialog.remove();
      apnDetailDialog = null;
    });
  });

  test('Element contains dialog', function() {
    const dialog = apnDetailDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    // Confirm that the dialog has the add apn title.
    assertEquals(
        apnDetailDialog.i18n('apnDetailAddApnDialogTitle'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailDialogTitle')
            .innerText);
  });

  test('Clicking the cancel button fires the close event', async function() {
    const closeEventPromise = eventToPromise('close', window);
    const cancelBtn =
        apnDetailDialog.shadowRoot.querySelector('#apnDetailCancelBtn');
    assertTrue(!!cancelBtn);

    cancelBtn.click();
    await closeEventPromise;
    assertFalse(!!apnDetailDialog.open);
  });
});
