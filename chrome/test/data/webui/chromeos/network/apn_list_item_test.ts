// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';

import type {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ApnListItemElement} from 'chrome://resources/ash/common/network/apn_list_item.js';
import type {ApnEventData} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {ApnDetailDialogMode} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {ApnProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ApnAuthenticationType, ApnIpType, ApnSource, ApnState, ApnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

const TEST_APN_EVENT_DATA_GUID = 'test_guid';

// TODO(crbug.com/367734332): Move into cellular_utils.ts.
const TEST_APN_EVENT_DATA: ApnEventData = {
  apn: {
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
  },
  mode: ApnDetailDialogMode.VIEW,
};

suite('ApnListItemTest', function() {
  let apnListItem: ApnListItemElement;

  let mojoApi_: FakeNetworkConfig;

  setup(() => {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi_);
  });

  async function init() {
    apnListItem = document.createElement('apn-list-item');
    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: 'apn1',
    });
    apnListItem.guid = 'cellular_guid';
    document.body.appendChild(apnListItem);
    return flushTasks();
  }

  function getThreeDotsMenu(): CrActionMenuElement {
    return apnListItem.shadowRoot!.querySelector<CrActionMenuElement>(
        '#dotsMenu')!;
  }

  async function openThreeDotMenu() {
    const menuButton =
        apnListItem.shadowRoot!.querySelector<CrIconButtonElement>(
            '#actionMenuButton');
    assertTrue(!!menuButton);
    assertFalse(getThreeDotsMenu().open);

    menuButton.click();
    return flushTasks();
  }

  // TODO(crbug.com/367734332): Move into cellular_utils.ts.
  function createTestApnWithOverridenValues(overrides: Partial<ApnProperties>):
      ApnProperties {
    return {...TEST_APN_EVENT_DATA.apn, ...overrides};
  }

  test('Check if APN list item exists', async () => {
    await init();
    assertTrue(!!apnListItem);
    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: 'apn1',
    });
    await flushTasks();
    assertEquals(
        apnListItem.shadowRoot!.querySelector<HTMLElement>(
                                   '#apnName')!.innerText,
        apnListItem.apn.accessPointName);

    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: apnListItem.apn.accessPointName,
      name: 'name',
    });
    await flushTasks();
    assertEquals(
        apnListItem.shadowRoot!.querySelector<HTMLElement>(
                                   '#apnName')!.innerText,
        apnListItem.apn.name);

    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: '',
    });
    await flushTasks();
    assertEquals(
        apnListItem.shadowRoot!.querySelector<HTMLElement>(
                                   '#apnName')!.innerText,
        apnListItem.i18n('apnNameModem'));
  });

  test('Check if connected sublabel is shown', async () => {
    await init();
    apnListItem.isApnConnected = false;
    await flushTasks();

    const subLabel =
        apnListItem.shadowRoot!.querySelector<HTMLElement>('#subLabel');
    assertTrue(!!subLabel);
    assertTrue(subLabel.hasAttribute('hidden'), 'fails to hide sublabel');
    apnListItem.isApnConnected = true;
    await flushTasks();

    assertFalse(subLabel.hasAttribute('hidden'), 'fails to show sublabel');
    assertFalse(
        subLabel.hasAttribute('warning'),
        'fails to add warning attribute in sublabel');
    assertEquals(
        apnListItem.i18n('NetworkHealthStateConnected'), subLabel.innerText);

    apnListItem.portalState = PortalState.kNoInternet;
    assertFalse(subLabel.hasAttribute('hidden'), 'fails to hide show');
    assertTrue(
        subLabel.hasAttribute('warning'),
        'fails to add warning a11y in sublabel');
    assertEquals(
        apnListItem.i18n('networkListItemConnectedNoConnectivity'),
        subLabel.innerText);
  });

  test('Check if APN three dot menu shows', async () => {
    await init();
    await openThreeDotMenu();
    assertTrue(getThreeDotsMenu().open);
  });

  test('Check disabled state.', async () => {
    await init();
    apnListItem.apn = createTestApnWithOverridenValues({
      state: ApnState.kDisabled,
      accessPointName: 'apn',
    });
    await flushTasks();
    assertFalse(apnListItem.hasAttribute('is-disabled_'));

    apnListItem.apn = createTestApnWithOverridenValues({
      state: ApnState.kEnabled,
      accessPointName: 'apn',
      id: '1',
    });
    await flushTasks();
    assertFalse(apnListItem.hasAttribute('is-disabled_'));

    apnListItem.apn = createTestApnWithOverridenValues({
      state: ApnState.kDisabled,
      accessPointName: 'apn',
      id: '1',
    });
    await flushTasks();
    assertTrue(apnListItem.hasAttribute('is-disabled_'));
  });

  test('Check if three dot menu remove APN works', async () => {
    await init();
    const guid = 'cellular_guid';
    await openThreeDotMenu();
    const getRemoveButton = () =>
        getThreeDotsMenu().querySelector<HTMLButtonElement>('#removeButton')!;
    assertFalse(!!getRemoveButton());

    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: 'name1',
      id: '1',
    });
    await flushTasks();
    assertTrue(!!getRemoveButton());
    assertFalse(!!getRemoveButton().disabled);

    apnListItem.shouldDisallowApnModification = true;
    assertTrue(!!getRemoveButton().disabled);

    apnListItem.shouldDisallowApnModification = false;
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const props = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'cellular');

    props.typeProperties.cellular = {
      customApnList: [{
        accessPointName: 'name1',
        id: '1',
      }],
    };
    mojoApi_.setManagedPropertiesForTest(props);
    let managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        1, managedProps.result.typeProperties.cellular.customApnList.length);
    getRemoveButton().click();
    await mojoApi_.whenCalled('removeCustomApn');

    managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        0, managedProps.result.typeProperties.cellular.customApnList.length);
    assertFalse(getThreeDotsMenu().open);
  });

  test('Check if three dot menu disable/enable APN works', async () => {
    await init();
    const guid = 'cellular_guid';
    await openThreeDotMenu();
    const getEnableButton = () =>
        getThreeDotsMenu().querySelector<HTMLButtonElement>('#enableButton')!;
    const getDisableButton = () =>
        getThreeDotsMenu().querySelector<HTMLButtonElement>('#disableButton')!;
    assertFalse(!!getEnableButton());
    assertFalse(!!getDisableButton());

    const createApn = (disabled: boolean) => {
      return createTestApnWithOverridenValues({
        accessPointName: 'name1',
        id: '1',
        state: disabled ? ApnState.kDisabled : ApnState.kEnabled,
      });
    };

    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const props = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'cellular');
    const disabledApn = createApn(/*disabled=*/ true);
    props.typeProperties.cellular = {
      customApnList: [disabledApn],
    };
    mojoApi_.setManagedPropertiesForTest(props);

    apnListItem.apn = disabledApn;
    await flushTasks();
    assertTrue(!!getEnableButton());
    assertFalse(!!getEnableButton().disabled);
    assertFalse(!!getDisableButton());

    apnListItem.shouldDisallowApnModification = true;
    assertTrue(!!getEnableButton().disabled);

    apnListItem.shouldDisallowApnModification = false;
    getEnableButton().click();
    await mojoApi_.whenCalled('modifyCustomApn');
    let managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        ApnState.kEnabled,
        managedProps.result.typeProperties.cellular.customApnList[0].state);
    assertFalse(getThreeDotsMenu().open);

    apnListItem.apn = createApn(/*disabled=*/ false);
    await flushTasks();
    assertTrue(!!getDisableButton());
    assertFalse(!!getDisableButton().disabled);
    assertFalse(!!getEnableButton());

    apnListItem.shouldDisallowApnModification = true;
    assertTrue(!!getDisableButton().disabled);

    apnListItem.shouldDisallowApnModification = false;
    getDisableButton().click();
    await mojoApi_.whenCalled('modifyCustomApn');
    managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        ApnState.kDisabled,
        managedProps.result.typeProperties.cellular.customApnList[0].state);
    assertFalse(getThreeDotsMenu().open);
  });

  [true, false].forEach(isApnRevampAndAllowApnModificationPolicyEnabled => {
    test(
        `Clicking APN details button triggers a show-apn-detail-dialog event
        when isApnRevampAndAllowApnModificationPolicyEnabled is ${
            isApnRevampAndAllowApnModificationPolicyEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            isApnRevampAndAllowApnModificationPolicyEnabled,
          });
          await init();
          apnListItem.apn = TEST_APN_EVENT_DATA.apn;
          apnListItem.guid = TEST_APN_EVENT_DATA_GUID;

          const subLabel = apnListItem.shadowRoot!.querySelector<HTMLElement>(
              '#autoDetected');
          assertTrue(!!subLabel);
          assertFalse(subLabel.hasAttribute('hidden'));
          assertEquals(apnListItem.i18n('apnAutoDetected'), subLabel.innerText);

          const getDetailsButton = () =>
              apnListItem.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#detailsButton')!;

          let apnDetailsClickedEvent =
              eventToPromise('show-apn-detail-dialog', window);
          assertTrue(!!getDetailsButton());
          assertEquals(
              apnListItem.i18n('apnMenuDetails'),
              getDetailsButton().innerText.trim());
          getDetailsButton().click();
          let eventData = await apnDetailsClickedEvent;

          assertEquals(TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
          assertEquals(TEST_APN_EVENT_DATA.mode, eventData.detail.mode);
          assertFalse(getThreeDotsMenu().open);

          // Case: the apn list item is not auto detected.
          apnListItem.apn = createTestApnWithOverridenValues({
            name: TEST_APN_EVENT_DATA.apn.name,
            id: '1',
          });
          assertTrue(subLabel.hasAttribute('hidden'));
          assertEquals(
              apnListItem.i18n('apnMenuEdit'),
              getDetailsButton().innerText.trim());

          apnDetailsClickedEvent =
              eventToPromise('show-apn-detail-dialog', window);
          getDetailsButton().click();
          eventData = await apnDetailsClickedEvent;
          assertEquals(TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
          assertEquals(ApnDetailDialogMode.EDIT, eventData.detail.mode);
          assertFalse(getThreeDotsMenu().open);

          if (isApnRevampAndAllowApnModificationPolicyEnabled) {
            // Case: APN modification is disallowed.
            apnListItem.shouldDisallowApnModification = true;
            await flushTasks();

            assertEquals(
                apnListItem.i18n('apnMenuDetails'),
                getDetailsButton().innerText.trim());

            apnDetailsClickedEvent =
                eventToPromise('show-apn-detail-dialog', window);
            getDetailsButton().click();
            eventData = await apnDetailsClickedEvent;
            assertEquals(
                TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
            assertEquals(ApnDetailDialogMode.VIEW, eventData.detail.mode);
            assertFalse(getThreeDotsMenu().open);
          }
        });
  });

  test('Test if disable/remove warning event is fired.', async () => {
    await init();
    const guid = 'cellular_guid';
    let promptShowEvent = eventToPromise('show-error-toast', window);
    await openThreeDotMenu();
    const getDisableButton = () =>
        getThreeDotsMenu().querySelector<HTMLButtonElement>('#disableButton')!;
    const getRemoveButton = () =>
        getThreeDotsMenu().querySelector<HTMLButtonElement>('#removeButton')!;
    const createApn = () => createTestApnWithOverridenValues({
      accessPointName: 'name1',
      id: '1',
      state: ApnState.kEnabled,
    });

    apnListItem.shouldDisallowDisablingRemoving = true;
    apnListItem.apn = createApn();
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const props = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'cellular');

    props.typeProperties.cellular = {customApnList: [createApn()]};
    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
    getDisableButton().click();
    let eventData = await promptShowEvent;
    let managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        ApnState.kEnabled,
        managedProps.result.typeProperties.cellular.customApnList[0].state);
    assertEquals(
        apnListItem.i18n('apnWarningPromptForDisableRemove'), eventData.detail);
    assertFalse(getThreeDotsMenu().open);

    promptShowEvent = eventToPromise('show-error-toast', window);
    getRemoveButton().click();
    eventData = await promptShowEvent;
    managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        1, managedProps.result.typeProperties.cellular.customApnList.length);
    assertEquals(
        apnListItem.i18n('apnWarningPromptForDisableRemove'), eventData.detail);
    assertFalse(getThreeDotsMenu().open);
  });

  test('Test if enable warning event is fired.', async () => {
    await init();
    const guid = 'cellular_guid';
    const promptShowEvent = eventToPromise('show-error-toast', window);
    await openThreeDotMenu();
    const getEnableButton = () =>
        getThreeDotsMenu().querySelector<HTMLButtonElement>('#enableButton')!;
    const createApn = () => createTestApnWithOverridenValues({
      accessPointName: 'name1',
      id: '1',
      state: ApnState.kDisabled,
    });

    apnListItem.shouldDisallowEnabling = true;
    apnListItem.apn = createApn();
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const props = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'cellular');

    props.typeProperties.cellular = {customApnList: [createApn()]};
    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
    getEnableButton().click();
    const eventData = await promptShowEvent;
    const managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        ApnState.kDisabled,
        managedProps.result.typeProperties.cellular.customApnList[0].state);
    assertEquals(
        apnListItem.i18n('apnWarningPromptForEnable'), eventData.detail);
    assertFalse(getThreeDotsMenu().open);
  });

  test('Item a11y', async () => {
    await init();
    apnListItem.itemIndex = 0;
    apnListItem.listSize = 1;

    // Enabled custom APN, non-connected.
    const apnName = 'apn1';
    const apnUserFriendlyName = 'userFriendlyNameApn1';
    const apnId = '1';
    const defaultTypeOnly = apnListItem.i18n('apnA11yDefaultApnOnly');
    const attachTypeOnly = apnListItem.i18n('apnA11yAttachApnOnly');
    const defaultAndAttach = apnListItem.i18n('apnA11yDefaultAndAttachApn');

    // Attach only APN.
    apnListItem.apn = createTestApnWithOverridenValues({
      id: apnId,
      accessPointName: apnName,
      apnTypes: [ApnType.kAttach],
    });

    const enabledText = apnListItem.i18n('apnA11yEnabled');
    const nameText = apnListItem.i18n(
        'apnA11yName', /*index=*/ 1, /*count=*/ 1, /*name=*/ 'apn1');

    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + enabledText + ' ' + attachTypeOnly,
        'Failed a11y for Attach only APN');

    // Attach and Default APN.
    apnListItem.apn = createTestApnWithOverridenValues({
      id: apnId,
      accessPointName: apnName,
      apnTypes: [ApnType.kDefault, ApnType.kAttach],
    });

    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + enabledText + ' ' + defaultAndAttach,
        'Failed a11y for Attach+Default APN');

    // Default only APN.
    apnListItem.apn = createTestApnWithOverridenValues({
      id: apnId,
      accessPointName: apnName,
      apnTypes: [ApnType.kDefault],
    });

    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + enabledText + ' ' + defaultTypeOnly,
        'Failed a11y for Default only APN');

    // Enabled custom APN, connected.
    apnListItem.isApnConnected = true;
    await flushTasks();

    const connectedText = apnListItem.i18n('apnA11yConnected');
    assertEquals(
        nameText + ' ' + connectedText + ' ' + defaultTypeOnly,
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        'Failed a11y for connected custom APN');

    // Disabled custom APN, non-connected.
    apnListItem.apn = createTestApnWithOverridenValues({
      id: apnId,
      accessPointName: apnName,
      state: ApnState.kDisabled,
    });
    apnListItem.isApnConnected = false;

    const disabledText = apnListItem.i18n('apnA11yDisabled');
    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + disabledText,
        'Failed a11y for disconnected disabled custom APN');

    // Enabled database APN, non-connected.
    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: apnName,
    });
    apnListItem.isApnConnected = false;
    const autoDetectedText = apnListItem.i18n('apnA11yAutoDetected');
    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + autoDetectedText + ' ' + enabledText,
        'Failed a11y for disconnected but enabled database APN');

    // Enabled database APN, connected.
    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: apnName,
    });
    apnListItem.isApnConnected = true;
    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + autoDetectedText + ' ' + connectedText,
        'Failed a11y for enabled database APN, connected');

    // Null APN, connected.
    apnListItem.apn = createTestApnWithOverridenValues({
      accessPointName: '',
      name: null,
    });
    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        apnListItem.i18n(
            'apnA11yName', /*index=*/ 1, /*count=*/ 1,
            apnListItem.i18n('apnNameModem')) +
            ' ' + autoDetectedText + ' ' + connectedText,
        'Failed a11y for null APN');

    // User friendly APN name same as APN has no text indicating as such.
    apnListItem.apn = createTestApnWithOverridenValues({
      id: apnId,
      accessPointName: apnName,
      name: apnName,
    });
    apnListItem.isApnConnected = true;
    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        nameText + ' ' + connectedText,
        'Failed a11y for user friendly APN name same as APN');

    // User friendly APN name different from APN has indicating text.
    const userFriendlyNameText = apnListItem.i18n(
        'apnA11yName', /*index=*/ 1, /*count=*/ 1,
        /*name=*/ 'userFriendlyNameApn1');
    apnListItem.apn = createTestApnWithOverridenValues({
      id: apnId,
      accessPointName: apnName,
      name: apnUserFriendlyName,
    });

    const indicateUserFriendlyNameText = apnListItem.i18n(
        'apnA11yUserFriendlyNameIndicator', apnUserFriendlyName, apnName);
    assertEquals(
        apnListItem.shadowRoot!
            .querySelector<CrIconButtonElement>(
                '#actionMenuButton')!.getAttribute('aria-label'),
        userFriendlyNameText + ' ' + connectedText + ' ' +
            indicateUserFriendlyNameText,
        'Failed a11y for user friendly APN name different from APN');
  });
});
