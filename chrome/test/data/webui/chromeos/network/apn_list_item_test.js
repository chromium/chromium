// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';

import {ApnDetailDialogMode, ApnEventData} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ApnState, ApnType, CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @type {!ApnEventData} */
const TEST_APN_EVENT_DATA = {
  apn: {
    name: 'test_apn',
  },
  guid: 'test-guid',
  mode: ApnDetailDialogMode.VIEW,
};

suite('ApnListItemTest', function() {
  /** @type {ApnListItemElement} */
  let apnListItem = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  setup(async function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  async function init() {
    apnListItem = document.createElement('apn-list-item');
    apnListItem.apn = {
      accessPointName: 'apn1',
    };
    apnListItem.guid = 'cellular_guid';
    document.body.appendChild(apnListItem);
    return flushTasks();
  }

  function openThreeDotMenu() {
    const menuButton =
        apnListItem.shadowRoot.querySelector('#actionMenuButton');
    assertTrue(!!menuButton);
    assertFalse(apnListItem.$.dotsMenu.open);

    menuButton.click();
    return flushTasks();
  }

  test('Check if APN list item exists', async function() {
    await init();
    assertTrue(!!apnListItem);
    apnListItem.apn = {
      accessPointName: 'apn1',
    };
    await flushTasks();
    assertEquals(
        apnListItem.$.apnName.innerText, apnListItem.apn.accessPointName);

    apnListItem.apn = {
      accessPointName: apnListItem.apn.accessPointName,
      name: 'name',
    };
    await flushTasks();
    assertEquals(apnListItem.$.apnName.innerText, apnListItem.apn.name);

    apnListItem.apn = {};
    await flushTasks();
    assertEquals(
        apnListItem.$.apnName.innerText, apnListItem.i18n('apnNameModem'));
  });

  test('Check if connected sublabel is shown', async function() {
    await init();
    apnListItem.isConnected = false;
    await flushTasks();

    const subLabel = apnListItem.shadowRoot.querySelector('#subLabel');
    assertTrue(!!subLabel);
    assertTrue(subLabel.hasAttribute('hidden'));
    apnListItem.isConnected = true;
    await flushTasks();

    assertFalse(subLabel.hasAttribute('hidden'));
    assertFalse(subLabel.hasAttribute('warning'));
    assertEquals(
        apnListItem.i18n('NetworkHealthStateConnected'), subLabel.innerText);

    apnListItem.portalState = PortalState.kNoInternet;
    assertFalse(subLabel.hasAttribute('hidden'));
    assertTrue(subLabel.hasAttribute('warning'));
    assertEquals(
        apnListItem.i18n('networkListItemConnectedNoConnectivity'),
        subLabel.innerText);
  });

  test('Check if APN three dot menu shows', async function() {
    await init();
    await openThreeDotMenu();
    assertTrue(apnListItem.$.dotsMenu.open);
  });

  test('Check disabled state.', async function() {
    await init();
    apnListItem.apn = {
      state: ApnState.kDisabled,
      accessPointName: 'apn',
    };
    await flushTasks();
    assertFalse(apnListItem.hasAttribute('is-disabled_'));

    apnListItem.apn = {
      state: ApnState.kEnabled,
      accessPointName: 'apn',
      id: '1',
    };
    await flushTasks();
    assertFalse(apnListItem.hasAttribute('is-disabled_'));

    apnListItem.apn = {
      state: ApnState.kDisabled,
      accessPointName: 'apn',
      id: '1',
    };
    await flushTasks();
    assertTrue(apnListItem.hasAttribute('is-disabled_'));
  });

  test('Check if three dot menu remove APN works', async function() {
    await init();
    const guid = 'cellular_guid';
    await openThreeDotMenu();
    const getRemoveButton = () =>
        apnListItem.$.dotsMenu.querySelector('#removeButton');
    assertFalse(!!getRemoveButton());

    apnListItem.apn = {
      accessPointName: 'name1',
      id: '1',
    };
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
    assertFalse(apnListItem.$.dotsMenu.open);
  });

  test('Check if three dot menu disable/enable APN works', async function() {
    await init();
    const guid = 'cellular_guid';
    await openThreeDotMenu();
    const getEnableButton = () =>
        apnListItem.$.dotsMenu.querySelector('#enableButton');
    const getDisableButton = () =>
        apnListItem.$.dotsMenu.querySelector('#disableButton');
    assertFalse(!!getEnableButton());
    assertFalse(!!getDisableButton());

    const createApn = (disabled) => {
      return {
        accessPointName: 'name1',
        id: '1',
        state: disabled ? ApnState.kDisabled : ApnState.kEnabled,
      };
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
    assertFalse(apnListItem.$.dotsMenu.open);

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
    assertFalse(apnListItem.$.dotsMenu.open);
  });

  [true, false].forEach(isApnRevampAndAllowApnModificationPolicyEnabled => {
    test(
        `Clicking APN details button triggers a show-apn-detail-dialog event
        when isApnRevampAndAllowApnModificationPolicyEnabled is ${
            isApnRevampAndAllowApnModificationPolicyEnabled}`,
        async function() {
          loadTimeData.overrideValues({
            isApnRevampAndAllowApnModificationPolicyEnabled,
          });
          await init();
          apnListItem.apn = TEST_APN_EVENT_DATA.apn;
          apnListItem.guid = TEST_APN_EVENT_DATA.guid;

          const subLabel =
              apnListItem.shadowRoot.querySelector('#autoDetected');
          assertTrue(!!subLabel);
          assertFalse(subLabel.hasAttribute('hidden'));
          assertEquals(apnListItem.i18n('apnAutoDetected'), subLabel.innerText);

          let apnDetailsClickedEvent =
              eventToPromise('show-apn-detail-dialog', window);
          assertTrue(!!apnListItem.$.detailsButton);
          assertEquals(
              apnListItem.i18n('apnMenuDetails'),
              apnListItem.$.detailsButton.innerText.trim());
          apnListItem.$.detailsButton.click();
          let eventData = await apnDetailsClickedEvent;

          assertEquals(TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
          assertEquals(TEST_APN_EVENT_DATA.mode, eventData.detail.mode);
          assertFalse(apnListItem.$.dotsMenu.open);

          // Case: the apn list item is not auto detected.
          apnListItem.apn = {
            name: TEST_APN_EVENT_DATA.apn.name,
            id: '1',
          };
          assertTrue(subLabel.hasAttribute('hidden'));
          assertEquals(
              apnListItem.i18n('apnMenuEdit'),
              apnListItem.$.detailsButton.innerText.trim());

          apnDetailsClickedEvent =
              eventToPromise('show-apn-detail-dialog', window);
          apnListItem.$.detailsButton.click();
          eventData = await apnDetailsClickedEvent;
          assertEquals(TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
          assertEquals(ApnDetailDialogMode.EDIT, eventData.detail.mode);
          assertFalse(apnListItem.$.dotsMenu.open);

          if (isApnRevampAndAllowApnModificationPolicyEnabled) {
            // Case: APN modification is disallowed.
            apnListItem.shouldDisallowApnModification = true;
            await flushTasks();

            assertEquals(
                apnListItem.i18n('apnMenuDetails'),
                apnListItem.$.detailsButton.innerText.trim());

            apnDetailsClickedEvent =
                eventToPromise('show-apn-detail-dialog', window);
            apnListItem.$.detailsButton.click();
            eventData = await apnDetailsClickedEvent;
            assertEquals(
                TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
            assertEquals(ApnDetailDialogMode.VIEW, eventData.detail.mode);
            assertFalse(apnListItem.$.dotsMenu.open);
          }
        });
  });

  test('Test if disable/remove warning event is fired.', async function() {
    await init();
    const guid = 'cellular_guid';
    let promptShowEvent = eventToPromise('show-error-toast', window);
    await openThreeDotMenu();
    const getDisableButton = () =>
        apnListItem.$.dotsMenu.querySelector('#disableButton');
    const getRemoveButton = () =>
        apnListItem.$.dotsMenu.querySelector('#removeButton');
    const createApn = () => {
      return {
        accessPointName: 'name1',
        id: '1',
        state: ApnState.kEnabled,
      };
    };

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
    assertFalse(apnListItem.$.dotsMenu.open);

    promptShowEvent = eventToPromise('show-error-toast', window);
    getRemoveButton().click();
    eventData = await promptShowEvent;
    managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        1, managedProps.result.typeProperties.cellular.customApnList.length);
    assertEquals(
        apnListItem.i18n('apnWarningPromptForDisableRemove'), eventData.detail);
    assertFalse(apnListItem.$.dotsMenu.open);
  });

  test('Test if enable warning event is fired.', async function() {
    await init();
    const guid = 'cellular_guid';
    const promptShowEvent = eventToPromise('show-error-toast', window);
    await openThreeDotMenu();
    const getEnableButton = () =>
        apnListItem.$.dotsMenu.querySelector('#enableButton');
    const createApn = () => {
      return {
        accessPointName: 'name1',
        id: '1',
        state: ApnState.kDisabled,
      };
    };

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
    assertFalse(apnListItem.$.dotsMenu.open);
  });

  test('Item a11y', async function() {
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
    apnListItem.apn = {
      id: apnId,
      accessPointName: apnName,
      apnTypes: [ApnType.kAttach],
    };

    const enabledText = apnListItem.i18n('apnA11yEnabled');
    const nameText = apnListItem.i18n(
        'apnA11yName', /*index=*/ 1, /*count=*/ 1, /*name=*/ 'apn1');
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + enabledText + ' ' + attachTypeOnly);

    // Attach and Default APN.
    apnListItem.apn = {
      id: apnId,
      accessPointName: apnName,
      apnTypes: [ApnType.kDefault, ApnType.kAttach],
    };

    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + enabledText + ' ' + defaultAndAttach);

    // Default only APN.
    apnListItem.apn = {
      id: apnId,
      accessPointName: apnName,
      apnTypes: [ApnType.kDefault],
    };

    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + enabledText + ' ' + defaultTypeOnly);

    // Enabled custom APN, connected.
    apnListItem.isConnected = true;

    const connectedText = apnListItem.i18n('apnA11yConnected');
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + connectedText + ' ' + defaultTypeOnly);

    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + connectedText + ' ' + defaultTypeOnly);

    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + connectedText + ' ' + defaultTypeOnly);

    // Disabled custom APN, non-connected.
    apnListItem.apn = {
      id: apnId,
      accessPointName: apnName,
      state: ApnState.kDisabled,
    };
    apnListItem.isConnected = false;

    const disabledText = apnListItem.i18n('apnA11yDisabled');
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + disabledText);

    // Enabled database APN, non-connected.
    apnListItem.apn = {
      accessPointName: apnName,
    };
    apnListItem.isConnected = false;
    const autoDetectedText = apnListItem.i18n('apnA11yAutoDetected');
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + autoDetectedText + ' ' + enabledText);

    // Enabled database APN, connected.
    apnListItem.apn = {
      accessPointName: apnName,
    };
    apnListItem.isConnected = true;
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + autoDetectedText + ' ' + connectedText);

    // Null APN, connected.
    apnListItem.apn = {};
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        apnListItem.i18n(
            'apnA11yName', /*index=*/ 1, /*count=*/ 1,
            apnListItem.i18n('apnNameModem')) +
            ' ' + autoDetectedText + ' ' + connectedText);

    // User friendly APN name same as APN has no text indicating as such.
    apnListItem.apn = {
      id: apnId,
      accessPointName: apnName,
      name: apnName,
    };
    apnListItem.isConnected = true;
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        nameText + ' ' + connectedText);

    // User friendly APN name different from APN has indicating text.
    const userFriendlyNameText = apnListItem.i18n(
        'apnA11yName', /*index=*/ 1, /*count=*/ 1,
        /*name=*/ 'userFriendlyNameApn1');
    apnListItem.apn = {
      id: apnId,
      accessPointName: apnName,
      name: apnUserFriendlyName,
    };

    const indicateUserFriendlyNameText = apnListItem.i18n(
        'apnA11yUserFriendlyNameIndicator', apnUserFriendlyName, apnName);
    assertEquals(
        apnListItem.$.actionMenuButton.ariaLabel,
        userFriendlyNameText + ' ' + connectedText + ' ' +
            indicateUserFriendlyNameText);
  });
});
