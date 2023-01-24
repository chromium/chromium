// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {ApnDetailDialogMode, ApnEventData} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnState, CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    apnListItem = document.createElement('apn-list-item');
    apnListItem.apn = {
      accessPointName: 'apn1',
    };
    apnListItem.guid = 'cellular_guid';
    document.body.appendChild(apnListItem);
    await flushTasks();
  });

  function openThreeDotMenu() {
    const menuButton =
        apnListItem.shadowRoot.querySelector('#actionMenuButton');
    assertTrue(!!menuButton);
    assertFalse(apnListItem.$.dotsMenu.open);

    menuButton.click();
    return flushTasks();
  }

  test('Check if APN list item exists', async function() {
    assertTrue(!!apnListItem);
    apnListItem.apn = {
      accessPointName: 'apn1',
    };
    await flushTasks();
    assertEquals(
        apnListItem.$.apnName.innerText, apnListItem.apn.accessPointName);
  });

  test('Check if connected sublabel is shown', async function() {
    apnListItem.isConnected = false;
    await flushTasks();

    const subLabel = apnListItem.shadowRoot.querySelector('#subLabel');
    assertTrue(!!subLabel);
    assertTrue(subLabel.hasAttribute('hidden'));
    apnListItem.isConnected = true;
    await flushTasks();

    assertFalse(subLabel.hasAttribute('hidden'));
    assertEquals(
        apnListItem.i18n('NetworkHealthStateConnected'), subLabel.innerText);
  });

  test('Check if APN three dot menu shows', async function() {
    await openThreeDotMenu();
    assertTrue(apnListItem.$.dotsMenu.open);
  });

  test('Check disabled state.', async function() {
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
  });

  test('Check if three dot menu disable/enable APN works', async function() {
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
    assertFalse(!!getDisableButton());
    getEnableButton().click();
    await mojoApi_.whenCalled('modifyCustomApn');
    let managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        ApnState.kEnabled,
        managedProps.result.typeProperties.cellular.customApnList[0].state);

    apnListItem.apn = createApn(/*disabled=*/ false);
    await flushTasks();
    assertTrue(!!getDisableButton());
    assertFalse(!!getEnableButton());
    getDisableButton().click();
    await mojoApi_.whenCalled('modifyCustomApn');
    managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        ApnState.kDisabled,
        managedProps.result.typeProperties.cellular.customApnList[0].state);
  });

  test(
      'Clicking APN details button triggers a show-apn-detail-dialog event ',
      async function() {
        apnListItem.apn = TEST_APN_EVENT_DATA.apn;
        apnListItem.guid = TEST_APN_EVENT_DATA.guid;

        const subLabel = apnListItem.shadowRoot.querySelector('#autoDetected');
        assertTrue(!!subLabel);
        assertFalse(subLabel.hasAttribute('hidden'));
        assertEquals(apnListItem.i18n('apnAutoDetected'), subLabel.innerText);

        let apnDetailsClickedEvent =
            eventToPromise('show-apn-detail-dialog', window);
        assertTrue(!!apnListItem.$.detailsButton);
        apnListItem.$.detailsButton.click();
        let eventData = await apnDetailsClickedEvent;

        assertEquals(TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
        assertEquals(TEST_APN_EVENT_DATA.mode, eventData.detail.mode);

        // Case: the apn list item is not auto detected
        apnListItem.apn = {
          name: TEST_APN_EVENT_DATA.apn.name,
          id: '1',
        };
        assertTrue(subLabel.hasAttribute('hidden'));

        apnDetailsClickedEvent =
            eventToPromise('show-apn-detail-dialog', window);
        apnListItem.$.detailsButton.click();
        eventData = await apnDetailsClickedEvent;
        assertEquals(TEST_APN_EVENT_DATA.apn.name, eventData.detail.apn.name);
        assertEquals(ApnDetailDialogMode.EDIT, eventData.detail.mode);
      });

  test('Test if disable/remove warning event is fired.', async function() {
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
    promptShowEvent = eventToPromise('show-error-toast', window);
    getRemoveButton().click();
    eventData = await promptShowEvent;
    managedProps = await mojoApi_.getManagedProperties(guid);
    assertEquals(
        1, managedProps.result.typeProperties.cellular.customApnList.length);
    assertEquals(
        apnListItem.i18n('apnWarningPromptForDisableRemove'), eventData.detail);
  });

  test('Test if enable warning event is fired.', async function() {
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
    // TODO(b/162365553): Add string to chromeos_string when it is approved by
    // writers.
    assertEquals(
        `Can't enable this APN. Add a default APN to attach to.`,
        eventData.detail);
  });
});
