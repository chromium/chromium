// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_selection_dialog.js';

import {ApnSelectionDialog} from 'chrome://resources/ash/common/network/apn_selection_dialog.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  test('Element contains dialog', () => {
    const dialog = apnSelectionDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
  });

  test('No apnList', () => {
    const apns = apnSelectionDialog.shadowRoot.querySelectorAll(
        'apn-selection-dialog-list-item');
    assertEquals(0, apns.length);
    const ironList = apnSelectionDialog.shadowRoot.querySelector('iron-list');
    assertNull(ironList.selectedItem);
  });

  test('Populated apnList', async () => {
    /** @type {ApnProperties} */
    const apn1 = {
      accessPointName: 'Access Point 1',
    };

    /** @type {ApnProperties} */
    const apn2 = {
      accessPointName: 'Access Point 2',
    };

    const apnList = [apn1, apn2];
    apnSelectionDialog.apnList = apnList;
    await flushTasks();

    const ironList = apnSelectionDialog.shadowRoot.querySelector('iron-list');
    assertEquals(2, ironList.items.length, `Iron list items don't match`);

    const listItems = apnSelectionDialog.shadowRoot.querySelectorAll(
        'apn-selection-dialog-list-item');
    assertEquals(
        apnList.length, listItems.length, `APN list lengths don't match`);
    assertTrue(OncMojo.apnMatch(apn1, listItems[0].apn));
    assertTrue(OncMojo.apnMatch(apn2, listItems[1].apn));
    assertNull(ironList.selectedItem);
    assertFalse(listItems[0].selected);
    assertFalse(listItems[1].selected);

    // Select the second APN.
    listItems[1].click();
    await flushTasks();
    assertTrue(OncMojo.apnMatch(apn2, ironList.selectedItem));
    assertFalse(listItems[0].selected);
    assertTrue(listItems[1].selected);

    // De-select the APN.
    listItems[1].click();
    await flushTasks();
    assertNull(ironList.selectedItem);
    assertFalse(listItems[0].selected);
    assertFalse(listItems[1].selected);
  });

  test('Clicking the cancel button fires the close event', async () => {
    const closeEventPromise = eventToPromise('close', window);
    const cancelBtn =
        apnSelectionDialog.shadowRoot.querySelector('.cancel-button');
    assertTrue(!!cancelBtn);

    cancelBtn.click();
    await closeEventPromise;
    const crDialogElement =
        apnSelectionDialog.shadowRoot.querySelector('#apnSelectionDialog');
    assertTrue(!!crDialogElement);
    assertFalse(crDialogElement.open);
  });
});
