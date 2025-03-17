// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_selection_dialog_list_item.js';

import type {ApnSelectionDialogListItem} from 'chrome://resources/ash/common/network/apn_selection_dialog_list_item.js';
import type {ApnProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ApnAuthenticationType, ApnIpType, ApnSource, ApnState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

// TODO(crbug.com/367734332): Move into cellular_utils.ts.
const TEST_APN_PROPERTIES: ApnProperties = {
  accessPointName: 'test_apn',
  username: null,
  password: null,
  authentication: ApnAuthenticationType.kAutomatic,
  ipType: ApnIpType.kAutomatic,
  apnTypes: [],
  state: ApnState.kEnabled,
  id: null,
  language: null,
  localizedName: null,
  name: null,
  attach: null,
  source: ApnSource.kUi,
};

suite('ApnSelectionDialogListItem', () => {
  let apnSelectionDialogListItem: ApnSelectionDialogListItem;

  setup(function() {
    apnSelectionDialogListItem =
        document.createElement('apn-selection-dialog-list-item');
    document.body.appendChild(apnSelectionDialogListItem);
    return waitAfterNextRender(apnSelectionDialogListItem);
  });

  teardown(() => {
    apnSelectionDialogListItem.remove();
  });

  // TODO(crbug.com/367734332): Move into cellular_utils.ts.
  function createTestApnWithOverridenValues(overrides: Partial<ApnProperties>):
      ApnProperties {
    return {...TEST_APN_PROPERTIES, ...overrides};
  }

  test('Name UI states', async () => {
    // No name field. Secondary label should be hidden.
    apnSelectionDialogListItem.apn = createTestApnWithOverridenValues({
      accessPointName: 'apn1',
    });
    await flushTasks();
    const getFriendlyApnName = () =>
        apnSelectionDialogListItem.shadowRoot!.querySelector<HTMLSpanElement>(
            '#friendlyApnName')!;
    const getSecondaryApnName = () =>
        apnSelectionDialogListItem.shadowRoot!.querySelector<HTMLSpanElement>(
            '#secondaryApnName')!;
    assertEquals(
        getFriendlyApnName().innerText,
        apnSelectionDialogListItem.apn.accessPointName);
    assertTrue(getSecondaryApnName().hidden);

    // Name field is same as accessPointName. Secondary label should be hidden.
    apnSelectionDialogListItem.apn = createTestApnWithOverridenValues({
      accessPointName: 'apn1',
      name: 'apn1',
    });
    await flushTasks();
    assertEquals(
        getFriendlyApnName().innerText, apnSelectionDialogListItem.apn.name);
    assertTrue(getSecondaryApnName().hidden);

    // Name field is different from accessPointName. Secondary label should be
    // shown.
    apnSelectionDialogListItem.apn = createTestApnWithOverridenValues({
      accessPointName: 'apn1',
      name: 'apn1_name',
    });
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
        apnSelectionDialogListItem.shadowRoot!.querySelector('#checkmark');
    assertNull(getCheckmark());

    apnSelectionDialogListItem.selected = true;
    await flushTasks();

    const checkmark = getCheckmark();
    assertTrue(!!checkmark);
    assertEquals(
        apnSelectionDialogListItem.i18n('apnSelectionDialogListItemSelected'),
        checkmark.ariaLabel);
  });
});
