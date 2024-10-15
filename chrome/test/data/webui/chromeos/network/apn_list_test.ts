// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';
import 'chrome://resources/ash/common/network/apn_detail_dialog.js';

import {ApnDetailDialogMode} from '//resources/ash/common/network/cellular_utils.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {LocalizedLinkElement} from 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import type {ApnListElement} from 'chrome://resources/ash/common/network/apn_list.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ActivationStateType, ApnAuthenticationType, ApnIpType, ApnSource, ApnState, ApnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import type {ApnProperties, ManagedApnList, ManagedCellularProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolicySource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnListTest', () => {
  let apnList: ApnListElement;

  const connectedApn = getApnProperties('Access Point', 'AP-name');

  const apn1 = getApnProperties('Access Point 1', 'AP-name-1');

  const apn2 = getApnProperties('Access Point 2', 'AP-name-2');

  const customApn1 = getApnProperties(
      'Custom Access Point 1', 'AP-name-custom-1', undefined, undefined, '1');

  const customApn2 = getApnProperties(
      'Custom Access Point 2', 'AP-name-custom-2', undefined, undefined, '2');

  const customApn3 = getApnProperties(
      'Custom Access Point 3', 'AP-name-custom-3', undefined, undefined, '3');

  const customApnAttachEnabled = getApnProperties(
      'Custom Access Point Attach Enabled', 'AP-name-custom-4',
      ApnState.kEnabled, [ApnType.kAttach], '4');

  const customApnAttachDisabled = getApnProperties(
      'Custom Access Point Attach Disabled', 'AP-name-custom-5',
      ApnState.kDisabled, [ApnType.kAttach], '5');

  const customApnDefaultEnabled = getApnProperties(
      'Custom Access Point Default Enabled', 'AP-name-custom-6',
      ApnState.kEnabled, [ApnType.kDefault], '6');

  const customApnDefaultDisabled = getApnProperties(
      'Custom Access Point Default Disabled', 'AP-name-custom-7',
      ApnState.kDisabled, [ApnType.kDefault], '7');

  const customApnDefaultEnabled2 = getApnProperties(
      'Custom Access Point Default Enabled2', 'AP-name-custom-8',
      ApnState.kEnabled, [ApnType.kDefault], '8');

  const customApnDefaultAttachEnabled = getApnProperties(
      'Custom Access Point Default and Attach Enabled', 'AP-name-custom-9',
      ApnState.kEnabled, [ApnType.kDefault, ApnType.kAttach], '9');

  const customApnDefaultAttachDisabled = getApnProperties(
      'Custom Access Point Default and Attach Disabled', 'AP-name-custom-10',
      ApnState.kDisabled, [ApnType.kDefault, ApnType.kAttach], '10');

  function getZeroStateContent(): HTMLDivElement|null {
    return apnList.shadowRoot!.querySelector<HTMLDivElement>(
        '#zeroStateContent');
  }

  function getApnSettingsZeroStateDescriptionWithAddLink():
      LocalizedLinkElement|null {
    const zeroStateContent = getZeroStateContent();
    assertTrue(!!zeroStateContent);
    return zeroStateContent.querySelector('localized-link');
  }

  function getApnProperties(
      accessPointName: string, name?: string,
      state: ApnState = ApnState.kDisabled, apnTypes: ApnType[] = [],
      id?: string, source: ApnSource = ApnSource.kUi): ApnProperties {
    return {
      accessPointName: accessPointName,
      id: id,
      authentication: ApnAuthenticationType.kAutomatic,
      language: undefined,
      localizedName: undefined,
      name: name,
      password: undefined,
      username: undefined,
      attach: undefined,
      state: state,
      ipType: ApnIpType.kAutomatic,
      apnTypes: apnTypes,
      source: source,
    };
  }

  function getManagedApnList(activeValue: ApnProperties[] = []):
      ManagedApnList {
    return {
      activeValue: activeValue,
      policySource: PolicySource.kNone,
      policyValue: undefined,
    };
  }

  function getManagedCellularProperties(
      apnList: ManagedApnList, customApnList?: ApnProperties[],
      connectedApn?: ApnProperties): ManagedCellularProperties {
    return {
      activationState: ActivationStateType.kUnknown,
      simLocked: false,
      allowRoaming: undefined,
      allowTextMessages: undefined,
      apnList: apnList,
      autoConnect: undefined,
      customApnList: customApnList,
      eid: undefined,
      esn: undefined,
      family: undefined,
      firmwareRevision: undefined,
      foundNetworks: undefined,
      hardwareRevision: undefined,
      homeProvider: undefined,
      iccid: undefined,
      imei: undefined,
      lastGoodApn: undefined,
      connectedApn: connectedApn,
      manufacturer: undefined,
      mdn: undefined,
      meid: undefined,
      min: undefined,
      modelId: undefined,
      networkTechnology: undefined,
      simLockType: '',
      paymentPortal: undefined,
      roamingState: undefined,
      selectedApn: undefined,
      servingOperator: undefined,
      signalStrength: 0,
      supportNetworkScan: false,
    };
  }

  setup(async () => {
    apnList = document.createElement('apn-list');
    document.body.appendChild(apnList);
    await flushTasks();
  });

  test('Check if APN description exists', async () => {
    assertTrue(!!apnList);
    let descriptionWithLink =
        apnList.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#descriptionWithLink');
    assertTrue(!!descriptionWithLink);
    assertEquals(
        apnList.i18nAdvanced('apnSettingsDescriptionWithLink').toString(),
        descriptionWithLink.localizedString.toString());

    let descriptionWithoutLink =
        apnList.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#descriptionNoLink');
    assertFalse(!!descriptionWithoutLink);

    apnList.shouldOmitLinks = true;
    await flushTasks();
    descriptionWithLink =
        apnList.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#descriptionWithLink');
    descriptionWithoutLink =
        apnList.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#descriptionNoLink');
    assertFalse(!!descriptionWithLink);
    assertTrue(!!descriptionWithoutLink);
    assertEquals(
        apnList.i18n('apnSettingsDescriptionNoLink').toString(),
        descriptionWithoutLink.innerHTML.trim());
    const apnDescription =
        apnList.shadowRoot!.querySelector<HTMLDivElement>('#apnDescription');
    assertTrue(!!apnDescription);
    assertEquals('assertive', apnDescription.ariaLive);
  });

  test('No managedCellularProperties', async () => {
    // Temporarily set |managedCellularProperties| to trigger a UI refresh.
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */ undefined,
        /* connectedApn= */ undefined);
    await flushTasks();
    assertEquals(
        0, apnList.shadowRoot!.querySelectorAll('apn-list-item').length);
    assertTrue(!!getZeroStateContent(), 'Expected zero state text to show');

    let apnDetailDialog =
        apnList.shadowRoot!.querySelector('apn-detail-dialog');

    apnList.shouldOmitLinks = false;
    await flushTasks();

    const localizedLink = getApnSettingsZeroStateDescriptionWithAddLink();
    assertTrue(!!localizedLink, 'No link is present');
    const testDetail = {event: {preventDefault: () => {}}};
    assertFalse(!!apnDetailDialog, 'Detail dialog shows when it should not');
    localizedLink.dispatchEvent(
        new CustomEvent('link-clicked', {bubbles: false, detail: testDetail}));
    await flushTasks();

    apnDetailDialog = apnList.shadowRoot!.querySelector('apn-detail-dialog');
    assertTrue(!!apnDetailDialog, 'Detail dialog does not show when it should');
    assertEquals(
        ApnDetailDialogMode.CREATE, apnDetailDialog.mode,
        'Detail dialog is not in create mode');
  });

  test('Error states', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */ undefined,
        /* connectedApn= */ undefined);
    await flushTasks();
    assertTrue(!!getZeroStateContent(), 'No zero state content is present');
    const localizedLink = getApnSettingsZeroStateDescriptionWithAddLink();
    assertTrue(!!localizedLink, 'No link is present');

    assertEquals(
        apnList.i18nAdvanced('apnSettingsZeroStateDescriptionWithAddLink')
            .toString(),
        localizedLink.localizedString.toString());
    const getErrorMessage = () =>
        apnList.shadowRoot!.querySelector('#errorMessageContainer');
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
    const getErrorMessageText = function() {
      const errorMessageContainer = getErrorMessage();
      assertTrue(!!errorMessageContainer);
      const errorMessage = errorMessageContainer.querySelector('#errorMessage');
      assertTrue(!!errorMessage);
      return errorMessage.innerHTML.trim();
    };
    assertEquals(
        apnList.i18n('apnSettingsDatabaseApnsErrorMessage'),
        getErrorMessageText());

    // Add an enabled custom APN.
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */[customApnDefaultEnabled],
        /* connectedApn= */ undefined);
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertTrue(!!getErrorMessage());
    assertEquals(
        apnList.i18n('apnSettingsCustomApnsErrorMessage'),
        getErrorMessageText());

    // Disable the custom APN.
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */[customApnDefaultDisabled],
        /* connectedApn= */ undefined);
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertTrue(!!getErrorMessage());
    assertEquals(
        apnList.i18n('apnSettingsDatabaseApnsErrorMessage'),
        getErrorMessageText());

    // Add a connected APN. The error should not show.
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList([connectedApn]),
        /* customApnList= */ undefined,
        /* connectedApn= */ connectedApn);
    await flushTasks();
    assertFalse(!!getZeroStateContent());
    assertFalse(!!getErrorMessage());
    const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(1, apns.length);
    assertTrue(!!apns[0]);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(apns[0].isConnected);
  });

  test('There is no Connected APN and no custom APNs', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */ undefined,
        /* connectedApn= */ undefined);
    await flushTasks();
    const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(0, apns.length);
    assertTrue(!!getZeroStateContent());
  });

  test('There are custom APNs and there is no Connected APN ', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */[customApn1, customApn2],
        /* connectedApn= */ undefined);
    await flushTasks();
    const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(2, apns.length);
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(OncMojo.apnMatch(apns[0].apn, customApn1));
    assertTrue(OncMojo.apnMatch(apns[1].apn, customApn2));
    assertFalse(apns[0].isConnected);
    assertFalse(!!getZeroStateContent());
  });

  test(
      'Connected APN is inside apnList and there are no custom APNs',
      async () => {
        apnList.managedCellularProperties = getManagedCellularProperties(
            /* apnList= */ getManagedApnList([apn1, apn2, connectedApn]),
            /* customApnList= */ undefined,
            /* connectedApn= */ connectedApn);
        await flushTasks();
        const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
        assertEquals(1, apns.length);
        assertTrue(!!apns[0]);
        assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
        assertTrue(apns[0].isConnected);
        assertFalse(!!getZeroStateContent());
      });

  test(
      'Connected APN is inside apnList and there are custom APNs.',
      async () => {
        apnList.managedCellularProperties = getManagedCellularProperties(
            /* apnList= */ getManagedApnList([apn1, apn2, connectedApn]),
            /* customApnList= */[customApn1, customApn2],
            /* connectedApn= */ connectedApn);
        await flushTasks();
        const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
        assertEquals(3, apns.length);
        assertTrue(!!apns[0]);
        assertTrue(!!apns[1]);
        assertTrue(!!apns[2]);
        assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
        assertTrue(OncMojo.apnMatch(apns[1].apn, customApn1));
        assertTrue(OncMojo.apnMatch(apns[2].apn, customApn2));
        assertTrue(apns[0].isConnected);
        assertFalse(!!getZeroStateContent());
      });

  test('Connected APN is inside custom APN list.', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList([apn1, apn2]),
        /* customApnList= */[customApn1, customApn2, customApn3],
        /* connectedApn= */ customApn3);
    await flushTasks();
    const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(3, apns.length);
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(!!apns[2]);
    assertTrue(OncMojo.apnMatch(apns[0].apn, customApn3));
    assertTrue(OncMojo.apnMatch(apns[1].apn, customApn1));
    assertTrue(OncMojo.apnMatch(apns[2].apn, customApn2));
    assertTrue(apns[0].isConnected);
    assertFalse(!!getZeroStateContent());
  });

  test('Connected APN is the only apn in custom APN list.', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */[connectedApn],
        /* connectedApn= */ connectedApn);
    await flushTasks();
    let apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(1, apns.length);
    assertTrue(!!apns[0]);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(apns[0].isConnected);
    assertFalse(!!getZeroStateContent());

    // Simulate the APN no longer being connected.
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */[connectedApn],
        /* connectedApn= */ undefined);
    await flushTasks();
    apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(1, apns.length);
    assertTrue(!!apns[0]);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertFalse(apns[0].isConnected);
    assertFalse(!!getZeroStateContent());
  });

  test(
      'Calling openApnDetailDialogInCreateMode() opens APN detail dialog',
      async () => {
        let apnDetailDialog =
            apnList.shadowRoot!.querySelector('apn-detail-dialog');
        apnList.guid = 'fake-guid';
        assertFalse(!!apnDetailDialog);
        apnList.openApnDetailDialogInCreateMode();
        await flushTasks();
        apnDetailDialog =
            apnList.shadowRoot!.querySelector('apn-detail-dialog');
        assertTrue(!!apnDetailDialog);
        assertEquals(ApnDetailDialogMode.CREATE, apnDetailDialog.mode);
        assertEquals(apnList.guid, apnDetailDialog.guid);
        assertFalse(!!apnDetailDialog.apnProperties);
        const getApnDetailCancelBtn = function() {
          const apnDetailCancelBtn =
              apnDetailDialog.shadowRoot!.querySelector<CrButtonElement>(
                  '#apnDetailCancelBtn');
          assertTrue(!!apnDetailCancelBtn);
          return apnDetailCancelBtn;
        };
        getApnDetailCancelBtn().click();
      });

  test('APN detail dialog has the correct list', async () => {
    apnList.guid = 'fake-guid';
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */[customApn1],
        /* connectedApn= */ connectedApn);
    await flushTasks();
    apnList.openApnDetailDialogInCreateMode();
    await flushTasks();

    const apnDetailDialog =
        apnList.shadowRoot!.querySelector('apn-detail-dialog');
    assertTrue(!!apnDetailDialog);
    assertTrue(!!apnDetailDialog.apnList);
    assertEquals(1, apnDetailDialog.apnList.length);
    assertTrue(!!apnDetailDialog.apnList[0]);
    assertTrue(OncMojo.apnMatch(apnDetailDialog.apnList[0], customApn1));

    // Case: Custom APN list is undefined
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */ undefined,
        /* connectedApn= */ connectedApn);
    assertTrue(!!apnDetailDialog.apnList);
    assertEquals(0, apnDetailDialog.apnList.length);
    const getApnDetailCancelBtn = function() {
      const apnDetailCancelBtn =
          apnDetailDialog.shadowRoot!.querySelector<CrButtonElement>(
              '#apnDetailCancelBtn');
      assertTrue(!!apnDetailCancelBtn);
      return apnDetailCancelBtn;
    };
    getApnDetailCancelBtn().click();

    // Case: Custom APN list has 2 items
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */[customApn1, customApn2],
        /* connectedApn= */ connectedApn);
    assertTrue(!!apnDetailDialog.apnList);
    assertEquals(2, apnDetailDialog.apnList.length);
    assertTrue(!!apnDetailDialog.apnList[0]);
    assertTrue(!!apnDetailDialog.apnList[1]);
    assertTrue(OncMojo.apnMatch(apnDetailDialog.apnList[0], customApn1));
    assertTrue(OncMojo.apnMatch(apnDetailDialog.apnList[1], customApn2));

    getApnDetailCancelBtn().click();

    // Case: Custom APN is connected.
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */[customApn1],
        /* connectedApn= */ connectedApn);
    assertTrue(!!apnDetailDialog.apnList);
    assertEquals(1, apnDetailDialog.apnList.length);
    assertTrue(!!apnDetailDialog.apnList[0]);
    assertTrue(OncMojo.apnMatch(apnDetailDialog.apnList[0], customApn1));

    getApnDetailCancelBtn().click();
  });

  test(
      'Calling openApnSelectionDialog() opens APN selection dialog',
      async () => {
        let apnSelectionDialog =
            apnList.shadowRoot!.querySelector('apn-selection-dialog');
        apnList.guid = 'fake-guid';
        assertFalse(!!apnSelectionDialog);
        apnList.openApnSelectionDialog();
        await flushTasks();
        apnSelectionDialog =
            apnList.shadowRoot!.querySelector('apn-selection-dialog');
        assertTrue(!!apnSelectionDialog);
        assertEquals(apnList.guid, apnSelectionDialog.guid);
        assertEquals(0, apnSelectionDialog.apnList.length);

        apnList.managedCellularProperties = getManagedCellularProperties(
            /* apnList= */ getManagedApnList(), /* customApnList= */ undefined,
            /* connectedApn= */ undefined);
        assertEquals(0, apnSelectionDialog.apnList.length);

        const modbApn: ApnProperties = getApnProperties(
            'Acess Point 1', undefined, undefined, [ApnType.kDefault],
            undefined, ApnSource.kModb);
        const modemApn: ApnProperties = getApnProperties(
            'Acess Point 2', undefined, undefined, [ApnType.kDefault],
            undefined, ApnSource.kModem);

        apnList.managedCellularProperties = getManagedCellularProperties(
            /* apnList= */ getManagedApnList([modbApn, modemApn]),
            /* customApnList= */ undefined, /* connectedApn= */ undefined);

        // Only APNs with source kModb should be present.
        assertEquals(1, apnSelectionDialog.apnList.length);
        assertTrue(!!apnSelectionDialog.apnList[0]);
        assertTrue(OncMojo.apnMatch(modbApn, apnSelectionDialog.apnList[0]));

        const cancelButton =
            apnSelectionDialog.shadowRoot!.querySelector<CrButtonElement>(
                '.cancel-button');
        assertTrue(!!cancelButton);
        cancelButton.click();
        await flushTasks();
        apnSelectionDialog =
            apnList.shadowRoot!.querySelector('apn-selection-dialog');
        assertFalse(!!apnSelectionDialog);
      });

  test('Show disable/remove/enable warning', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */
        [
          Object.assign({}, connectedApn),
          Object.assign({}, customApnDefaultEnabled),
          Object.assign({}, customApnAttachDisabled),
        ],
        /* connectedApn= */ Object.assign({}, connectedApn));
    await flushTasks();
    let apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(!!apns[2]);
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
    assertFalse(apns[2].shouldDisallowDisablingRemoving);

    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */
        [
          Object.assign({}, connectedApn),
          Object.assign({}, customApnAttachEnabled),
          Object.assign({}, customApnDefaultEnabled),
          Object.assign({}, customApnAttachDisabled),
        ],
        /* connectedApn= */ Object.assign({}, connectedApn));
    await flushTasks();
    apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(!!apns[2]);
    assertTrue(!!apns[3]);
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
    assertTrue(apns[2].shouldDisallowDisablingRemoving);
    assertFalse(apns[3].shouldDisallowDisablingRemoving);

    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        [
          Object.assign({}, connectedApn),
          Object.assign({}, customApnAttachEnabled),
          Object.assign({}, customApnDefaultEnabled),
          Object.assign({}, customApnDefaultEnabled2),
          Object.assign({}, customApnAttachDisabled),
        ],
        /* connectedApn= */ Object.assign({}, connectedApn));
    await flushTasks();
    apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(!!apns[2]);
    assertTrue(!!apns[3]);
    assertTrue(!!apns[4]);
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
    assertFalse(apns[2].shouldDisallowDisablingRemoving);
    assertFalse(apns[3].shouldDisallowDisablingRemoving);
    assertFalse(apns[4].shouldDisallowDisablingRemoving);

    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */
        [
          Object.assign({}, customApnDefaultEnabled),
          Object.assign({}, customApnAttachEnabled),
        ],
        /* connectedApn= */ Object.assign({}, customApnDefaultEnabled));
    await flushTasks();
    apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(apns[0].shouldDisallowDisablingRemoving);
    assertFalse(apns[1].shouldDisallowDisablingRemoving);
  });

  test('Show enable warning', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */
        [
          connectedApn,
          customApnAttachDisabled,
          customApnDefaultEnabled,
          customApnAttachDisabled,
        ],
        /* connectedApn= */ connectedApn);
    await flushTasks();
    let apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(!!apns[2]);
    assertTrue(!!apns[3]);
    assertTrue(apns[0].shouldDisallowEnabling);
    assertFalse(apns[1].shouldDisallowEnabling);
    assertFalse(apns[2].shouldDisallowEnabling);
    assertFalse(apns[3].shouldDisallowEnabling);

    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */
        [
          connectedApn,
          customApnDefaultDisabled,
          customApnAttachDisabled,
        ],
        /* connectedApn= */ connectedApn);
    await flushTasks();
    apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertTrue(!!apns[2]);
    assertTrue(apns[0].shouldDisallowEnabling);
    assertFalse(apns[1].shouldDisallowEnabling);
    assertTrue(apns[2].shouldDisallowEnabling);

    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(),
        /* customApnList= */
        [
          customApnDefaultEnabled,
          customApnAttachDisabled,
        ],
        /* connectedApn= */ customApnDefaultEnabled);
    await flushTasks();
    apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertTrue(!!apns[0]);
    assertTrue(!!apns[1]);
    assertFalse(apns[0].shouldDisallowEnabling);
    assertFalse(apns[1].shouldDisallowEnabling);
  });

  test('Portal state is set', async () => {
    apnList.managedCellularProperties = getManagedCellularProperties(
        /* apnList= */ getManagedApnList(), /* customApnList= */[customApn1],
        /* connectedApn= */ undefined);
    await flushTasks();
    const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
    assertEquals(1, apns.length);
    assertTrue(!!apns[0]);
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
                    const testDbApn: ApnProperties = {
                      accessPointName: 'apn',
                      username: 'username',
                      password: 'password',
                      authentication: ApnAuthenticationType.kAutomatic,
                      ipType: ApnIpType.kAutomatic,
                      name: 'name',
                      apnTypes: discoveredApnTypes,
                      id: 'id',
                      source: ApnSource.kModb,
                      state: ApnState.kEnabled,
                      language: undefined,
                      localizedName: undefined,
                      attach: undefined,
                    };
                    let apnSelectionDialog = apnList.shadowRoot!.querySelector(
                        'apn-selection-dialog');
                    apnList.guid = 'fake-guid';
                    assertFalse(!!apnSelectionDialog);
                    apnList.openApnSelectionDialog();
                    await flushTasks();
                    apnSelectionDialog = apnList.shadowRoot!.querySelector(
                        'apn-selection-dialog');
                    assertTrue(!!apnSelectionDialog);
                    assertEquals(apnList.guid, apnSelectionDialog.guid);
                    assertEquals(0, apnSelectionDialog.apnList.length);

                    apnList.managedCellularProperties =
                        getManagedCellularProperties(
                            /* apnList= */ getManagedApnList(),
                            /* customApnList= */ undefined,
                            /* connectedApn= */ undefined);
                    assertEquals(0, apnSelectionDialog.apnList.length);

                    apnList.managedCellularProperties =
                        getManagedCellularProperties(
                            /* apnList= */ getManagedApnList([testDbApn]),
                            /* customApnList= */ customApnList,
                            /* connectedApn= */ undefined);

                    assertEquals(
                        scenario.shouldShowApn,
                        apnSelectionDialog.apnList.length === 1,
                        `APN should be displayed`);
                    if (scenario.shouldShowApn) {
                      assertTrue(!!apnSelectionDialog.apnList[0]);
                      assertTrue(OncMojo.apnMatch(
                          testDbApn, apnSelectionDialog.apnList[0]));
                    }
                  });
            }));

    test('ShouldDisallowApnModification is set', async () => {
      apnList.managedCellularProperties = getManagedCellularProperties(
          /* apnList= */ getManagedApnList(), /* customApnList= */[customApn1],
          /* connectedApn= */ undefined);
      await flushTasks();
      const apns = apnList.shadowRoot!.querySelectorAll('apn-list-item');
      assertEquals(1, apns.length);
      assertTrue(!!apns[0]);
      assertTrue(OncMojo.apnMatch(apns[0].apn, customApn1));
      assertFalse(!!apns[0].shouldDisallowApnModification);

      apnList.shouldDisallowApnModification = true;
      assertTrue(!!apns[0].shouldDisallowApnModification);
    });
  });
});
