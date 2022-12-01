// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnState, CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnListItemTest', function() {
  /** @type {ApnListItemElement} */
  let apnListItem = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  setup(async function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    apnListItem = document.createElement('apn-list-item');
    apnListItem.apn = {name: 'apn1'};
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

  test('Check if auto detected label is shown', async function() {
    apnListItem.isAutoDetected = false;
    await flushTasks();

    const subLabel = apnListItem.shadowRoot.querySelector('#autoDetected');
    assertTrue(!!subLabel);
    assertTrue(subLabel.hasAttribute('hidden'));
    apnListItem.isAutoDetected = true;
    await flushTasks();

    assertFalse(subLabel.hasAttribute('hidden'));
    assertEquals(apnListItem.i18n('apnAutoDetected'), subLabel.innerText);
  });

  test('Check if APN three dot menu shows', async function() {
    await openThreeDotMenu();
    assertTrue(apnListItem.$.dotsMenu.open);
  });

  test('Check disabled state.', async function() {
    apnListItem.apn = {state: ApnState.kDisabled, name: 'apn'};
    await flushTasks();
    assertFalse(apnListItem.hasAttribute('is-disabled_'));

    apnListItem.apn = {state: ApnState.kEnabled, name: 'apn', id: '1'};
    await flushTasks();
    assertFalse(apnListItem.hasAttribute('is-disabled_'));

    apnListItem.apn = {state: ApnState.kDisabled, name: 'apn', id: '1'};
    await flushTasks();
    assertTrue(apnListItem.hasAttribute('is-disabled_'));
  });

  test('Remove APN', async function() {
    const guid = 'cellular_guid';
    await openThreeDotMenu();
    const getRemoveButton = () =>
        apnListItem.$.dotsMenu.querySelector('#removeButton');
    assertTrue(getRemoveButton().hidden);

    apnListItem.apn = {name: 'name1', id: '1'};
    await flushTasks();
    assertFalse(getRemoveButton().hidden);

    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const props = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'cellular');

    props.typeProperties.cellular = {customApnList: [{name: 'name1', id: '1'}]};
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
});
