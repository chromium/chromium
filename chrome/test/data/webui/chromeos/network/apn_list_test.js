// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {ApnDetailDialogMode} from '//resources/ash/common/network/cellular_utils.js';
import {ApnListElement} from 'chrome://resources/ash/common/network/apn_list.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, ApnSource, ApnState, ApnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnListTest', function() {
  /** @type {ApnListElement} */
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

  /** @type {ApnProperties} */
  const customApnDefaultAttachEnabled = {
    accessPointName: 'Custom Access Point Default and Attach Enabled',
    name: 'AP-name-custom-9',
    apnTypes: [ApnType.kDefault, ApnType.kAttach],
    state: ApnState.kEnabled,
    id: '9',
  };

  /** @type {ApnProperties} */
  const customApnDefaultAttachDisabled = {
    accessPointName: 'Custom Access Point Default and Attach Disabled',
    name: 'AP-name-custom-10',
    apnTypes: [ApnType.kDefault, ApnType.kAttach],
    state: ApnState.kDisabled,
    id: '10',
  };

  function getZeroStateContent() {
    return apnList.shadowRoot.getElementById('zeroStateContent');
  }

  function getApnSettingsZeroStateDescriptionWithAddLink() {
    return getZeroStateContent().querySelector('localized-link');
  }

  setup(async function() {
    apnList = document.createElement('apn-list');
    document.body.appendChild(apnList);
    await flushTasks();
  });

  test('Check if APN description exists', async function() {
    assertTrue(!!apnList);
    const getDescriptionWithLink = () =>
        apnList.shadowRoot.querySelector('#descriptionWithLink');
    assertTrue(!!getDescriptionWithLink());
    assertEquals(
        getDescriptionWithLink().localizedString.toString(),
        apnList.i18nAdvanced('apnSettingsDescriptionWithLink').toString());

    const getDescriptionWithoutLink = () =>
        apnList.shadowRoot.querySelector('#descriptionNoLink');
    assertFalse(!!getDescriptionWithoutLink());

    apnList.shouldOmitLinks = true;
    await flushTasks();
    assertFalse(!!getDescriptionWithLink());
    assertTrue(!!getDescriptionWithoutLink());
    assertEquals(
        getDescriptionWithoutLink().innerHTML.trim(),
        apnList.i18n('apnSettingsDescriptionNoLink').toString());
    assertEquals(
        'assertive',
        apnList.shadowRoot.querySelector('#apnDescription').ariaLive);
  });

  test('No managedCellularProperties', async function() {
    // Temporarily set |managedCellularProperties| to trigger a UI refresh.
    apnList.managedCellularProperties = {};
    apnList.managedCellularProperties = undefined;
    await flushTasks();
    assertEquals(
        apnList.shadowRoot.querySelectorAll('apn-list-item').length, 0);
    assertTrue(!!getZeroStateContent(), 'Expected zero state text to show');

    const getApnDetailDialog = () =>
        apnList.shadowRoot.querySelector('apn-detail-dialog');

    apnList.shouldOmitLinks = false;
    await flushTasks();

    const localizedLink = getApnSettingsZeroStateDescriptionWithAddLink();
    assertTrue(!!localizedLink, 'No link is present');
    const testDetail = {event: {preventDefault: () => {}}};
    assertFalse(
        !!getApnDetailDialog(), 'Detail dialog shows when it should not');
    localizedLink.dispatchEvent(
        new CustomEvent('link-clicked', {bubbles: false, detail: testDetail}));
    await flushTasks();

    assertTrue(
        !!getApnDetailDialog(), 'Detail dialog does not show when it should');
    assertEquals(
        ApnDetailDialogMode.CREATE, getApnDetailDialog().mode,
        'Detail dialog is not in create mode');
  });

  test('Error states', async function() {
    apnList.managedCellularProperties = {};
    await flushTasks();
    assertTrue(!!getZeroStateContent(), 'No zero state content is present');
    assertTrue(
        !!getApnSettingsZeroStateDescriptionWithAddLink(),
        'No link is present');

    assertEquals(
        getApnSettingsZeroStateDescriptionWithAddLink()
            .localizedString.toString(),
        apnList.i18nAdvanced('apnSettingsZeroStateDescriptionWithAddLink')
            .toString());
    const getErrorMessage = () =>
        apnList.shadowRoot.querySelector('#errorMessageContainer');
    assertFalse(!!getErrorMessage());

    // Set as non-APN-related error.
    apnList.errorState = 'connect-failed';
    await flushTasks();
    assertTrue(!!getZeroStateContent());
    assertFalse(!!getErrorMessage());

    // Set as APN-related error.
    apnList.errorState = 'invalid-apn';
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertTrue(!!getErrorMessage());
    const getErrorMessageText = () =>
        getErrorMessage().querySelector('#errorMessage').innerHTML.trim();
    assertEquals(
        apnList.i18n('apnSettingsDatabaseApnsErrorMessage'),
        getErrorMessageText());

    // Add an enabled custom APN.
    apnList.managedCellularProperties = {
      customApnList: [customApnDefaultEnabled],
    };
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertTrue(!!getErrorMessage());
    assertEquals(
        apnList.i18n('apnSettingsCustomApnsErrorMessage'),
        getErrorMessageText());

    // Disable the custom APN.
    apnList.managedCellularProperties = {
      customApnList: [customApnDefaultDisabled],
    };
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertTrue(!!getErrorMessage());
    assertEquals(
        apnList.i18n('apnSettingsDatabaseApnsErrorMessage'),
        getErrorMessageText());

    // Add a connected APN. The error should not show.
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      apnList: {
        activeValue: [connectedApn],
      },
    };
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertFalse(!!getErrorMessage());
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(apns[0].isConnected);
  });

  test('There is no Connected APN and no custom APNs', async function() {
    apnList.managedCellularProperties = {};
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 0);
    assertTrue(!!getZeroStateContent());
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
        assertFalse(!!getZeroStateContent());
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
        assertFalse(!!getZeroStateContent());
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
        assertFalse(!!getZeroStateContent());
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
    assertFalse(!!getZeroStateContent());
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
    assertFalse(!!getZeroStateContent());

    // Simulate the APN no longer being connected.
    apnList.managedCellularProperties = {
      customApnList: [connectedApn],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertFalse(apns[0].isConnected);
    assertFalse(!!getZeroStateContent());
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

    // Case: Custom APN is connected.
    apnList.managedCellularProperties = {
      connectedApn: customApn1,
      customApnList: [customApn1],
    };
    assertTrue(!!getApnDetailDialog().apnList);
    assertEquals(1, getApnDetailDialog().apnList.length);
    assertTrue(OncMojo.apnMatch(getApnDetailDialog().apnList[0], customApn1));

    assertTrue(
        !!getApnDetailDialog().shadowRoot.querySelector('#apnDetailCancelBtn'));
    getApnDetailDialog()
        .shadowRoot.querySelector('#apnDetailCancelBtn')
        .click();
  });

  test(
      'Calling openApnSelectionDialog() opens APN selection dialog',
      async function() {
        const getApnSelectionDialog = () =>
            apnList.shadowRoot.querySelector('apn-selection-dialog');
        apnList.guid = 'fake-guid';
        assertFalse(!!getApnSelectionDialog());
        apnList.openApnSelectionDialog();
        await flushTasks();
        assertTrue(!!getApnSelectionDialog());
        assertEquals(apnList.guid, getApnSelectionDialog().guid);
        assertEquals(0, getApnSelectionDialog().apnList.length);

        apnList.managedCellularProperties = {};
        assertEquals(0, getApnSelectionDialog().apnList.length);

        const modbApn = {
          accessPointName: 'Access Point 1',
          source: ApnSource.kModb,
          apnTypes: [ApnType.kDefault],
        };
        const modemApn = {
          accessPointName: 'Access Point 2',
          source: ApnSource.kModem,
          apnTypes: [ApnType.kDefault],
        };
        apnList.managedCellularProperties = {
          apnList: {
            activeValue: [
              modbApn,
              modemApn,
            ],
          },
        };

        // Only APNs with source kModb should be present.
        assertEquals(1, getApnSelectionDialog().apnList.length);
        assertTrue(
            OncMojo.apnMatch(modbApn, getApnSelectionDialog().apnList[0]));

        const cancelButton =
            getApnSelectionDialog().shadowRoot.querySelector('.cancel-button');
        assertTrue(!!cancelButton);
        cancelButton.click();
        await flushTasks();

        assertFalse(!!getApnSelectionDialog());
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

    apnList.managedCellularProperties = {
      connectedApn: Object.assign({}, customApnDefaultEnabled),
      customApnList: [
        Object.assign({}, customApnDefaultEnabled),
        Object.assign({}, customApnAttachEnabled),
      ],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
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

    apnList.managedCellularProperties = {
      connectedApn: customApnDefaultEnabled,
      customApnList: [
        customApnDefaultEnabled,
        customApnAttachDisabled,
      ],
    };
    await flushTasks();
    apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertFalse(apns[0].shouldDisallowEnabling);
    assertFalse(apns[1].shouldDisallowEnabling);
  });

  test('Portal state is set', async function() {
    apnList.managedCellularProperties = {
      customApnList: [customApn1],
    };
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, customApn1));
    assertFalse(!!apns[0].portalState);

    apnList.portalState = PortalState.kNoInternet;
    assertEquals(PortalState.kNoInternet, apns[0].portalState);
  });

  [{
    shouldShowApn: true,
    apnTypesOfDatabaseApn: [
      [ApnType.kDefault],
      [ApnType.kAttach],
      [ApnType.kDefault, ApnType.kAttach],
      [ApnType.kDefault, ApnType.kTether],
    ],
    customApnLists: [
      [customApnDefaultEnabled],
      [customApnDefaultAttachEnabled],
    ],
  },
   {
     shouldShowApn: true,
     apnTypesOfDatabaseApn: [
       [ApnType.kDefault],
       [ApnType.kDefault, ApnType.kAttach],
       [ApnType.kDefault, ApnType.kTether],
       [ApnType.kDefault, ApnType.kAttach, ApnType.kTether],
     ],
     customApnLists: [
       [],
       [customApnDefaultDisabled],
       [customApnDefaultAttachDisabled],
     ],
   },
   {
     shouldShowApn: false,
     apnTypesOfDatabaseApn: [
       [ApnType.kAttach],
       [ApnType.kAttach, ApnType.kTether],
     ],
     customApnLists: [
       [],
       [customApnDefaultDisabled],
       [customApnDefaultAttachDisabled],
     ],
   }].forEach(scenario => {
    scenario.apnTypesOfDatabaseApn.forEach(
        (discoveredApnTypes) =>
            scenario.customApnLists.forEach((customApnList) => {
              test(
                  'When existing custom APNs are ' +
                      JSON.stringify(customApnList) +
                      ' and the single database APN has the APN types of ' +
                      JSON.stringify(discoveredApnTypes) + ', the APN should ' +
                      (scenario.shouldShowApn ? 'be shown' : 'not be shown'),
                  async () => {
                    /** @type {ApnProperties} */
                    const testDbApn = {
                      accessPointName: 'apn',
                      name: 'name',
                      apnTypes: discoveredApnTypes,
                      id: 'id',
                      source: ApnSource.kModb,
                    };
                    const getApnSelectionDialog = () =>
                        apnList.shadowRoot.querySelector(
                            'apn-selection-dialog');
                    apnList.guid = 'fake-guid';
                    assertFalse(!!getApnSelectionDialog());
                    apnList.openApnSelectionDialog();
                    await flushTasks();
                    assertTrue(!!getApnSelectionDialog());
                    assertEquals(apnList.guid, getApnSelectionDialog().guid);
                    assertEquals(0, getApnSelectionDialog().apnList.length);

                    apnList.managedCellularProperties = {};
                    assertEquals(0, getApnSelectionDialog().apnList.length);

                    apnList.managedCellularProperties = {
                      customApnList: customApnList,
                      apnList: {
                        activeValue: [testDbApn],
                      },
                    };

                    assertEquals(
                        scenario.shouldShowApn,
                        getApnSelectionDialog().apnList.length === 1,
                        `APN should be displayed`);
                    if (scenario.shouldShowApn) {
                      assertTrue(OncMojo.apnMatch(
                          testDbApn, getApnSelectionDialog().apnList[0]));
                    }
                  });
            }));

    test('ShouldDisallowApnModification is set', async function() {
      apnList.managedCellularProperties = {
        customApnList: [customApn1],
      };
      await flushTasks();
      const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
      assertEquals(apns.length, 1);
      assertTrue(OncMojo.apnMatch(apns[0].apn, customApn1));
      assertFalse(!!apns[0].shouldDisallowApnModification);

      apnList.shouldDisallowApnModification = true;
      assertTrue(!!apns[0].shouldDisallowApnModification);
    });
  });
});
