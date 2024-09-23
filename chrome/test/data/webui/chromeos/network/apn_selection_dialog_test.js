// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_selection_dialog.js';

import {ApnSelectionDialog} from 'chrome://resources/ash/common/network/apn_selection_dialog.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @type {!ApnProperties} */
const TEST_APN = {
  accessPointName: 'apn',
  username: 'username',
  password: 'password',
  authenticationType: ApnAuthenticationType.kAutomatic,
  ipType: ApnIpType.kAutomatic,
  apnTypes: [ApnType.kDefault],
};

suite('ApnSelectionDialog', () => {
  /** @type {ApnSelectionDialog} */
  let apnSelectionDialog = null;

  /** @type {FakeNetworkConfig} */
  let mojoApi = null;

  setup(function() {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
    apnSelectionDialog = document.createElement('apn-selection-dialog');
    apnSelectionDialog.guid = 'fake-guid';
    apnSelectionDialog.shouldOmitLinks = false;
    document.body.appendChild(apnSelectionDialog);
    return waitAfterNextRender(apnSelectionDialog);
  });

  teardown(() => {
    apnSelectionDialog.remove();
    mojoApi.resetForTest();
  });

  test('Element contains dialog', async () => {
    const dialog = apnSelectionDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    const apnSelectionDialogTitle =
        apnSelectionDialog.shadowRoot.querySelector('#apnSelectionDialogTitle');
    assertTrue(!!apnSelectionDialogTitle, 'Title does not exist');
    assertEquals(
        apnSelectionDialog.i18n('apnSelectionDialogTitle'),
        apnSelectionDialogTitle.innerText, 'Inner text does not match');

    const getDescriptionWithLink = () =>
        apnSelectionDialog.shadowRoot.querySelector('localized-link');
    assertTrue(
        !!getDescriptionWithLink(),
        'Description does not contain link when it should');

    const apnSelectionActionBtn =
        apnSelectionDialog.shadowRoot.querySelector('#apnSelectionActionBtn');
    assertTrue(!!apnSelectionActionBtn);
    assertEquals(
        apnSelectionDialog.i18n('apnSelectionDialogUseApn'),
        apnSelectionActionBtn.innerText);

    const apnSelectionCancelBtn =
        apnSelectionDialog.shadowRoot.querySelector('#apnSelectionCancelBtn');
    assertTrue(!!apnSelectionCancelBtn);
    assertEquals(
        apnSelectionDialog.i18n('apnDetailDialogCancel'),
        apnSelectionCancelBtn.innerText,
    );
    assertEquals(
        apnSelectionCancelBtn, apnSelectionDialog.shadowRoot.activeElement);

    apnSelectionDialog.shouldOmitLinks = true;
    await flushTasks();
    assertFalse(
        !!getDescriptionWithLink(),
        'Description contains link when it should not');
    const apnSelectionDialogDescription =
        apnSelectionDialog.shadowRoot.querySelector(
            '#apnSelectionDialogDescription');
    assertTrue(!!apnSelectionDialogDescription, 'Description does not show');
    assertEquals(
        apnSelectionDialog.i18n('apnSelectionDialogDescription'),
        apnSelectionDialogDescription.innerText,
        'Description does not contain expected text');
    assertEquals('polite', apnSelectionDialogDescription.ariaLive);
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

    // Button state should not be announced when dialog opens initially.
    // Announcement should only be made when the enabled state changes
    // from disabled to enabled.
    const getActionButtonEnabledA11yText = () =>
        apnSelectionDialog.shadowRoot.querySelector(
            '#actionButtonEnabledA11yText');
    assertFalse(!!getActionButtonEnabledA11yText());

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
    assertEquals('assertive', listItems[0].ariaLive);
    assertEquals('assertive', listItems[1].ariaLive);
    assertNull(ironList.selectedItem);
    assertFalse(listItems[0].selected);
    assertEquals('false', listItems[0].ariaSelected);
    assertFalse(listItems[1].selected);
    assertEquals('false', listItems[1].ariaSelected);
    assertFalse(!!getActionButtonEnabledA11yText());

    // Select the second APN.
    listItems[1].click();
    await flushTasks();
    assertTrue(OncMojo.apnMatch(apn2, ironList.selectedItem));
    assertFalse(listItems[0].selected);
    assertEquals('false', listItems[0].ariaSelected);
    assertTrue(listItems[1].selected);
    assertEquals('true', listItems[1].ariaSelected);

    // Button state becomes enabled, announcement should be made.
    assertTrue(!!getActionButtonEnabledA11yText());
    assertEquals(
        apnSelectionDialog.i18n('apnSelectionDialogA11yUseApnEnabled'),
        getActionButtonEnabledA11yText().innerText);

    // De-select the APN.
    listItems[1].click();
    await flushTasks();
    assertNull(ironList.selectedItem, `List has a non-null selected item`);
    assertFalse(
        listItems[0].selected, `apn1 is selected when it shouldn\'t be`);
    assertEquals('false', listItems[0].ariaSelected);
    assertFalse(
        listItems[1].selected, `apn2 is selected when it shouldn\'t be`);
    assertEquals('false', listItems[0].ariaSelected);

    // Button state becomes disabled, announcement should be made.
    assertTrue(!!getActionButtonEnabledA11yText());
    assertEquals(
        apnSelectionDialog.i18n('apnSelectionDialogA11yUseApnDisabled'),
        getActionButtonEnabledA11yText().innerText);
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

  test('Clicking on use this APN button adds APN', async () => {
    const apnList = [TEST_APN];
    apnSelectionDialog.apnList = apnList;
    await flushTasks();

    const actionBtn =
        apnSelectionDialog.shadowRoot.querySelector('.action-button');
    const cancelBtn =
        apnSelectionDialog.shadowRoot.querySelector('.cancel-button');

    assertTrue(!!actionBtn, `action button does not exist`);
    assertTrue(!!cancelBtn, `cancel button does not exist`);

    // No APN selected, so action button is disabled.
    const ironList = apnSelectionDialog.shadowRoot.querySelector('iron-list');
    assertEquals(1, ironList.items.length, `Iron list items don't match`);
    assertTrue(actionBtn.disabled, `action button is not disabled`);
    assertFalse(cancelBtn.disabled, `cancel button is disabled`);

    const listItems = apnSelectionDialog.shadowRoot.querySelectorAll(
        'apn-selection-dialog-list-item');

    // Select the known APN.
    listItems[0].click();
    assertTrue(OncMojo.apnMatch(TEST_APN, ironList.selectedItem));

    assertFalse(actionBtn.disabled);
    assertFalse(cancelBtn.disabled);

    // Case: clicking on the action button calls the correct method
    const network = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, apnSelectionDialog.guid);
    mojoApi.setManagedPropertiesForTest(network);
    await flushTasks();

    /**
     * @type {!ManagedProperties}
     */
    const managedProperties =
        await mojoApi.getManagedProperties(apnSelectionDialog.guid);
    assertTrue(!!managedProperties);
    assertFalse(
        !!managedProperties.result.typeProperties.cellular.customApnList);

    const closeEventPromise = eventToPromise('close', window);
    actionBtn.click();
    await flushTasks();
    await mojoApi.whenCalled('createExclusivelyEnabledCustomApn');

    await closeEventPromise;
    const crDialogElement =
        apnSelectionDialog.shadowRoot.querySelector('#apnSelectionDialog');
    assertTrue(!!crDialogElement);
    assertFalse(crDialogElement.open);

    assertEquals(
        1,
        managedProperties.result.typeProperties.cellular.customApnList.length);

    const apn =
        managedProperties.result.typeProperties.cellular.customApnList[0];
    assertEquals(TEST_APN.accessPointName, apn.accessPointName);
    assertEquals(TEST_APN.username, apn.username);
    assertEquals(TEST_APN.password, apn.password);
    assertEquals(TEST_APN.authenticationType, apn.authenticationType);
    assertEquals(TEST_APN.ipType, apn.ipType);
    assertEquals(TEST_APN.apnTypes.length, apn.apnTypes.length);
    assertEquals(TEST_APN.apnTypes[0], apn.apnTypes[0]);
  });
});
