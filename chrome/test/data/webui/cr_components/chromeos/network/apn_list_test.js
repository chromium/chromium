// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {ApnDetailDialogMode} from '//resources/ash/common/network/cellular_utils.js';
import {ApnList} from 'chrome://resources/ash/common/network/apn_list.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, ApnState, ApnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnListTest', function() {
  /** @type {ApnList} */
  let apnList = null;

  /** @type {ApnProperties} */
  const connectedApn = {
    accessPointName: 'Access Point',
    name: 'AP-name',
  };
  /** @type {ApnProperties} */
  const apn1 = {
    accessPointName: 'Access Point 1',
    name: 'AP-name-1',
  };

  /** @type {ApnProperties} */
  const apn2 = {
    accessPointName: 'Access Point 2',
    name: 'AP-name-2',
  };

  /** @type {ApnProperties} */
  const customApn1 = {
    accessPointName: 'Custom Access Point 1',
    name: 'AP-name-custom-1',
    id: '1',
  };

  /** @type {ApnProperties} */
  const customApn2 = {
    accessPointName: 'Custom Access Point 2',
    name: 'AP-name-custom-2',
    id: '2',
  };

  /** @type {ApnProperties} */
  const customApn3 = {
    accessPointName: 'Custom Access Point 3',
    name: 'AP-name-custom-3',
    id: '3',
  };

  /** @type {ApnProperties} */
  const customApnAttachEnabled = {
    accessPointName: 'Custom Access Point Attach Enabled',
    name: 'AP-name-custom-4',
    apnTypes: [ApnType.kAttach],
    state: ApnState.kEnabled,
    id: '4',
  };

  /** @type {ApnProperties} */
  const customApnAttachDisabled = {
    accessPointName: 'Custom Access Point Attach Disabled',
    name: 'AP-name-custom-5',
    apnTypes: [ApnType.kAttach],
    state: ApnState.kDisabled,
    id: '5',
  };

  /** @type {ApnProperties} */
  const customApnDefaultEnabled = {
    accessPointName: 'Custom Access Point Default Enabled',
    name: 'AP-name-custom-6',
    apnTypes: [ApnType.kDefault],
    state: ApnState.kEnabled,
    id: '6',
  };

  /** @type {ApnProperties} */
  const customApnDefaultDisabled = {
    accessPointName: 'Custom Access Point Default Disabled',
    name: 'AP-name-custom-7',
    apnTypes: [ApnType.kDefault],
    state: ApnState.kDisabled,
    id: '7',
  };

  /** @type {ApnProperties} */
  const customApnDefaultEnabled2 = {
    accessPointName: 'Custom Access Point Default Enabled2',
    name: 'AP-name-custom-8',
    apnTypes: [ApnType.kDefault],
    state: ApnState.kEnabled,
    id: '8',
  };

  function getZeroStateText() {
    return apnList.shadowRoot.querySelector('#zeroStateText');
  }

  setup(async function() {
    apnList = document.createElement('apn-list');
    document.body.appendChild(apnList);
    await flushTasks();
  });

  test('Check if APN description exists', async function() {
    assertTrue(!!apnList);
    const getDescriptionWithLink = () =>
        apnList.shadowRoot.querySelector('localized-link');
    assertTrue(!!getDescriptionWithLink());
    const getDescriptionWithoutLink = () =>
        apnList.shadowRoot.querySelector('#descriptionNoLink');
    assertFalse(!!getDescriptionWithoutLink());

    apnList.shouldOmitLinks = true;
    await flushTasks();
    assertFalse(!!getDescriptionWithLink());
    assertTrue(!!getDescriptionWithoutLink());
  });

  test('No managedCellularProperties', async function() {
    // Temporarily set |managedCellularProperties| to trigger a UI refresh.
    apnList.managedCellularProperties = {};
    apnList.managedCellularProperties = undefined;
    await flushTasks();
    assertEquals(
        apnList.shadowRoot.querySelectorAll('apn-list-item').length, 0);
    assertTrue(!!getZeroStateText());
  });

  test('Error states', async function() {
    apnList.managedCellularProperties = {};
    await flushTasks();
    assertTrue(!!getZeroStateText());
    assertEquals(
        apnList.i18n('apnSettingsZeroStateDescription'),
        getZeroStateText().querySelector('div').innerText);
    const getErrorMessage = () =>
        apnList.shadowRoot.querySelector('#errorMessage');
    assertFalse(!!getErrorMessage());

    // Set as non-APN-related error.
    apnList.errorState = 'connect-failed';
    await flushTasks();
    assertTrue(!!getZeroStateText());
    assertFalse(!!getErrorMessage());

    // Set as APN-related error.
    apnList.errorState = 'invalid-apn';
    await flushTasks();
    assertFalse(!!getZeroStateText());
    assertTrue(!!getErrorMessage());
    const getErrorMessageText = () =>
        getErrorMessage().querySelector('localized-link').localizedString;
    assertEquals('Can\'t connect to network.', getErrorMessageText());

    // Add an enabled custom APN.
    apnList.managedCellularProperties = {
      customApnList: [customApnDefaultEnabled],
    };
    await flushTasks();
    assertFalse(!!getZeroStateText());
    assertTrue(!!getErrorMessage());
    assertEquals(
        apnList.i18n('apnSettingsCustomApnsErrorMessage'),
        getErrorMessageText());

    // Disable the custom APN.
    apnList.managedCellularProperties = {
      customApnList: [customApnDefaultDisabled],
    };
    await flushTasks();
    assertFalse(!!getZeroStateText());
    assertTrue(!!getErrorMessage());
    assertEquals('Can\'t connect to network.', getErrorMessageText());
  });

  test('There is no Connected APN and no custom APNs', async function() {
    apnList.managedCellularProperties = {};
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 0);
    assertTrue(!!getZeroStateText());
  });

  test(
      'There are custom APNs and there is no Connected APN ', async function() {
        apnList.managedCellularProperties = {
          customApnList: [customApn1, customApn2],
        };
        await flushTasks();
        const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
        assertEquals(apns.length, 2);
        assertTrue(OncMojo.apnMatch(apns[0].apn, customApn1));
        assertTrue(OncMojo.apnMatch(apns[1].apn, customApn2));
        assertFalse(apns[0].isConnected);
        assertFalse(!!getZeroStateText());
      });

  test(
      'Connected APN is inside apnList and there are no custom APNs',
      async function() {
        apnList.managedCellularProperties = {
          connectedApn: connectedApn,
          apnList: {
            activeValue: [apn1, apn2, connectedApn],
          },
        };
        await flushTasks();
        const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
        assertEquals(apns.length, 1);
        assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
        assertTrue(apns[0].isConnected);
        assertFalse(!!getZeroStateText());
      });

  test(
      'Connected APN is inside apnList and there are custom APNs.',
      async function() {
        apnList.managedCellularProperties = {
          connectedApn: connectedApn,
          apnList: {
            activeValue: [apn1, apn2, connectedApn],
          },
          customApnList: [customApn1, customApn2],
        };
        await flushTasks();
        const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
        assertEquals(apns.length, 3);
        assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
        assertTrue(OncMojo.apnMatch(apns[1].apn, customApn1));
        assertTrue(OncMojo.apnMatch(apns[2].apn, customApn2));
        assertTrue(apns[0].isConnected);
        assertFalse(!!getZeroStateText());
      });

  test('Connected APN is inside custom APN list.', async function() {
    apnList.managedCellularProperties = {
      connectedApn: customApn3,
      apnList: {
        activeValue: [apn1, apn2],
      },
      customApnList: [customApn1, customApn2, customApn3],
    };
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 3);
    assertTrue(OncMojo.apnMatch(apns[0].apn, customApn3));
    assertTrue(OncMojo.apnMatch(apns[1].apn, customApn1));
    assertTrue(OncMojo.apnMatch(apns[2].apn, customApn2));
    assertTrue(apns[0].isConnected);
    assertFalse(!!getZeroStateText());
  });

  test('Connected APN is the only apn in custom APN list.', async function() {
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: [connectedApn],
    };
    await flushTasks();
    let apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(apns[0].isConnected);
    assertFalse(!!getZeroStateText());

    // Simulate the APN no longer being connected.
    apnList.managedCellularProperties = {
      customApnList: [connectedApn],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertFalse(apns[0].isConnected);
    assertFalse(!!getZeroStateText());
  });

  test(
      'Calling openApnDetailDialogInCreateMode() opens APN detail dialog',
      async function() {
        const getApnDetailDialog = () =>
            apnList.shadowRoot.querySelector('apn-detail-dialog');
        apnList.guid = 'fake-guid';
        assertFalse(!!getApnDetailDialog());
        apnList.openApnDetailDialogInCreateMode();
        await flushTasks();
        assertTrue(!!getApnDetailDialog());
        assertEquals(ApnDetailDialogMode.CREATE, getApnDetailDialog().mode);
        assertEquals(apnList.guid, getApnDetailDialog().guid);
        assertFalse(!!getApnDetailDialog().apnProperties);
        assertTrue(!!getApnDetailDialog().shadowRoot.querySelector(
            '#apnDetailCancelBtn'));
        getApnDetailDialog()
            .shadowRoot.querySelector('#apnDetailCancelBtn')
            .click();
      });

  test('APN detail dialog has the correct list', async () => {
    const getApnDetailDialog = () =>
        apnList.shadowRoot.querySelector('apn-detail-dialog');
    apnList.guid = 'fake-guid';
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: [customApn1],
    };
    await flushTasks();
    apnList.openApnDetailDialogInCreateMode();
    await flushTasks();

    assertTrue(!!getApnDetailDialog());
    assertTrue(!!getApnDetailDialog().apnList);
    assertTrue(getApnDetailDialog().apnList.length === 1);
    assertTrue(OncMojo.apnMatch(getApnDetailDialog().apnList[0], customApn1));

    // Case: Custom APN list is undefined
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: undefined,
    };
    assertTrue(!!getApnDetailDialog().apnList);
    assertTrue(getApnDetailDialog().apnList.length === 0);
    assertTrue(
        !!getApnDetailDialog().shadowRoot.querySelector('#apnDetailCancelBtn'));
    getApnDetailDialog()
        .shadowRoot.querySelector('#apnDetailCancelBtn')
        .click();

    // Case: Custom APN list has 2 items
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: [customApn1, customApn2],
    };
    assertTrue(!!getApnDetailDialog().apnList);
    assertTrue(getApnDetailDialog().apnList.length === 2);
    assertTrue(OncMojo.apnMatch(getApnDetailDialog().apnList[0], customApn1));
    assertTrue(OncMojo.apnMatch(getApnDetailDialog().apnList[1], customApn2));

    assertTrue(
        !!getApnDetailDialog().shadowRoot.querySelector('#apnDetailCancelBtn'));
    getApnDetailDialog()
        .shadowRoot.querySelector('#apnDetailCancelBtn')
        .click();
  });

  test('Show disable/remove/enable warning', async function() {
    apnList.managedCellularProperties = {
      connectedApn: Object.assign({}, connectedApn),
      customApnList: [
        Object.assign({}, connectedApn),
        Object.assign({}, customApnDefaultEnabled),
        Object.assign({}, customApnAttachDisabled),
      ],
    };
    await flushTasks();
    let apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
    assertFalse(apns[2].shouldDisallowDisablingRemoving);

    apnList.managedCellularProperties = {
      connectedApn: Object.assign({}, connectedApn),
      customApnList: [
        Object.assign({}, connectedApn),
        Object.assign({}, customApnAttachEnabled),
        Object.assign({}, customApnDefaultEnabled),
        Object.assign({}, customApnAttachDisabled),
      ],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
    assertTrue(apns[2].shouldDisallowDisablingRemoving);
    assertFalse(apns[3].shouldDisallowDisablingRemoving);

    apnList.managedCellularProperties = {
      connectedApn: Object.assign({}, connectedApn),
      customApnList: [
        Object.assign({}, connectedApn),
        Object.assign({}, customApnAttachEnabled),
        Object.assign({}, customApnDefaultEnabled),
        Object.assign({}, customApnDefaultEnabled2),
        Object.assign({}, customApnAttachDisabled),
      ],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
    assertFalse(apns[2].shouldDisallowDisablingRemoving);
    assertFalse(apns[3].shouldDisallowDisablingRemoving);
    assertFalse(apns[4].shouldDisallowDisablingRemoving);
  });

  test('Show enable warning', async function() {
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: [
        connectedApn,
        customApnAttachDisabled,
        customApnDefaultEnabled,
        customApnAttachDisabled,
      ],
    };
    await flushTasks();
    let apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertTrue(apns[0].shouldDisallowEnabling);
    assertFalse(apns[1].shouldDisallowEnabling);
    assertFalse(apns[2].shouldDisallowEnabling);
    assertFalse(apns[3].shouldDisallowEnabling);

    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: [
        connectedApn,
        customApnDefaultDisabled,
        customApnAttachDisabled,
      ],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertTrue(apns[0].shouldDisallowEnabling);
    assertFalse(apns[1].shouldDisallowEnabling);
    assertTrue(apns[2].shouldDisallowEnabling);
  });
});
