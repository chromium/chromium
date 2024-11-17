// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_selection_dialog_list_item.js';

import {ApnSelectionDialogListItem} from 'chrome://resources/ash/common/network/apn_selection_dialog_list_item.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnSelectionDialogListItem', () => {
  /** @type {ApnSelectionDialogListItem} */
  let apnSelectionDialogListItem = null;

  setup(function() {
    apnSelectionDialogListItem =
        document.createElement('apn-selection-dialog-list-item');
    document.body.appendChild(apnSelectionDialogListItem);
    return waitAfterNextRender(apnSelectionDialogListItem);
  });

  teardown(() => {
    apnSelectionDialogListItem.remove();
  });

  test('Name UI states', async () => {
    // No name field. Secondary label should be hidden.
    apnSelectionDialogListItem.apn = {
      accessPointName: 'apn1',
    };
    await flushTasks();
    const getFriendlyApnName = () =>
        apnSelectionDialogListItem.$.friendlyApnName;
    const getSecondaryApnName = () =>
        apnSelectionDialogListItem.$.secondaryApnName;
    assertEquals(
        getFriendlyApnName().innerText,
        apnSelectionDialogListItem.apn.accessPointName);
    assertTrue(getSecondaryApnName().hidden);

    // Name field is same as accessPointName. Secondary label should be hidden.
    apnSelectionDialogListItem.apn = {
      accessPointName: 'apn1',
      name: 'apn1',
    };
    await flushTasks();
    assertEquals(
        getFriendlyApnName().innerText, apnSelectionDialogListItem.apn.name);
    assertTrue(getSecondaryApnName().hidden);

    // Name field is different from accessPointName. Secondary label should be
    // shown.
    apnSelectionDialogListItem.apn = {
      accessPointName: 'apn1',
      name: 'apn1_name',
    };
    await flushTasks();
    assertEquals(
        getFriendlyApnName().innerText.trim(),
        apnSelectionDialogListItem.apn.name);
    assertFalse(getSecondaryApnName().hidden);
    assertEquals(
        getSecondaryApnName().innerText.trim(),
        apnSelectionDialogListItem.apn.accessPointName);
  });

  test('Item selected', async () => {
    const getCheckmark = () =>
        apnSelectionDialogListItem.shadowRoot.querySelector('#checkmark');
    assertNull(getCheckmark());

    apnSelectionDialogListItem.selected = true;
    await flushTasks();
    assertTrue(!!getCheckmark);
    assertEquals(
        apnSelectionDialogListItem.i18n('apnSelectionDialogListItemSelected'),
        getCheckmark().ariaLabel);
  });
});
