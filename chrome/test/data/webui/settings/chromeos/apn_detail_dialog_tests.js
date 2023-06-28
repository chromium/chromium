// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_detail_dialog.js';

import {ApnDetailDialog} from 'chrome://resources/ash/common/network/apn_detail_dialog.js';
import {ApnDetailDialogMode} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnState, ApnType, ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @type {!ApnProperties} */
const TEST_APN = {
  accessPointName: 'apn',
  username: 'username',
  password: 'password',
  authentication: ApnAuthenticationType.kAutomatic,
  ipType: ApnIpType.kAutomatic,
  apnTypes: [ApnType.kDefault],
  state: ApnState.kEnabled,
};

suite('ApnDetailDialog', function() {
  /** @type {ApnDetailDialog} */
  let apnDetailDialog = null;

  let mojoApi_ = null;

  async function toggleAdvancedSettings() {
    const advancedSettingsBtn =
        apnDetailDialog.shadowRoot.querySelector('#advancedSettingsBtn');
    assertTrue(!!advancedSettingsBtn);
    advancedSettingsBtn.click();
  }

  /**
   * @param {string} selector
   */
  function assertElementEnabled(selector) {
    const element = apnDetailDialog.shadowRoot.querySelector(selector);
    assertTrue(!!element);
    assertFalse(element.disabled);
  }

  function assertAllInputsEnabled() {
    assertElementEnabled('#apnInput');
    assertElementEnabled('#usernameInput');
    assertElementEnabled('#passwordInput');
    assertElementEnabled('#authTypeDropDown');
    assertElementEnabled('#apnDefaultTypeCheckbox');
    assertElementEnabled('#apnAttachTypeCheckbox');
    assertElementEnabled('#ipTypeDropDown');
  }

  setup(async function() {
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    apnDetailDialog = document.createElement('apn-detail-dialog');
    apnDetailDialog.guid = 'fake-guid';
    apnDetailDialog.apnList = [TEST_APN];
  });

  teardown(function() {
    return flushTasks().then(() => {
      apnDetailDialog.remove();
      apnDetailDialog = null;
    });
  });

  async function init() {
    document.body.appendChild(apnDetailDialog);
    return waitAfterNextRender(apnDetailDialog);
  }

  test('Element contains dialog', async function() {
    await init();
    const dialog = apnDetailDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    // Confirm that the dialog has the add apn title.
    assertEquals(
        apnDetailDialog.i18n('apnDetailAddApnDialogTitle'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailDialogTitle')
            .innerText);
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#apnInput'));
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#usernameInput'));
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#passwordInput'));

    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#authTypeDropDown'));
    const defaultTypeCheckbox =
        apnDetailDialog.shadowRoot.querySelector('#apnDefaultTypeCheckbox');
    assertTrue(!!defaultTypeCheckbox);
    assertTrue(defaultTypeCheckbox.checked);
    assertTrue(
        !!apnDetailDialog.shadowRoot.querySelector('#apnAttachTypeCheckbox'));
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#ipTypeDropDown'));
    assertTrue(
        !!apnDetailDialog.shadowRoot.querySelector('#apnDetailCancelBtn'));
    assertTrue(
        !!apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn'));
    assertFalse(!!apnDetailDialog.shadowRoot.querySelector('#apnDoneBtn'));
    assertEquals(
        apnDetailDialog.shadowRoot.querySelector('#apnInput'),
        apnDetailDialog.shadowRoot.activeElement);
  });

  test('Clicking the cancel button fires the close event', async function() {
    await init();
    const closeEventPromise = eventToPromise('close', window);
    const cancelBtn =
        apnDetailDialog.shadowRoot.querySelector('#apnDetailCancelBtn');
    assertTrue(!!cancelBtn);

    cancelBtn.click();
    await closeEventPromise;
    assertFalse(!!apnDetailDialog.$.apnDetailDialog.open);
  });

  test(
      'Clicking on the advanced settings button expands/collapses section',
      async function() {
        await init();
        const isAdvancedSettingShowing = () =>
            apnDetailDialog.shadowRoot.querySelector('iron-collapse').opened;
        assertFalse(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        assertTrue(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        assertFalse(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        const assertOptions = (expectedTextArray, optionNodes) => {
          for (const [idx, expectedText] of expectedTextArray.entries()) {
            assertTrue(!!optionNodes[idx]);
            assertTrue(!!optionNodes[idx].text);
            assertEquals(expectedText, optionNodes[idx].text);
          }
        };
        const authTypeDropDown =
            apnDetailDialog.shadowRoot.querySelector('#authTypeDropDown');
        assertTrue(!!authTypeDropDown);
        const authTypeOptionNodes = authTypeDropDown.querySelectorAll('option');
        assertEquals(3, authTypeOptionNodes.length);
        // Note: We are also checking that the items appear in a certain order.
        assertOptions(
            [
              apnDetailDialog.i18n('apnDetailTypeAuto'),
              apnDetailDialog.i18n('apnDetailAuthTypePAP'),
              apnDetailDialog.i18n('apnDetailAuthTypeCHAP'),
            ],
            authTypeOptionNodes);

        const ipTypeDropDown =
            apnDetailDialog.shadowRoot.querySelector('#ipTypeDropDown');
        assertTrue(!!ipTypeDropDown);
        const ipTypeOptionNodes = ipTypeDropDown.querySelectorAll('option');
        assertEquals(4, ipTypeOptionNodes.length);

        assertOptions(
            [
              apnDetailDialog.i18n('apnDetailTypeAuto'),
              apnDetailDialog.i18n('apnDetailIpTypeIpv4'),
              apnDetailDialog.i18n('apnDetailIpTypeIpv6'),
              apnDetailDialog.i18n('apnDetailIpTypeIpv4_Ipv6'),
            ],
            ipTypeOptionNodes);
      });

  test('Clicking on the add button calls createCustomApn', async () => {
    await init();
    apnDetailDialog.$.apnInput.value = TEST_APN.accessPointName;
    apnDetailDialog.$.usernameInput.value = TEST_APN.username;
    apnDetailDialog.$.passwordInput.value = TEST_APN.password;

    assertAllInputsEnabled();
    assertElementEnabled('#apnDetailCancelBtn');
    assertElementEnabled('#apnDetailActionBtn');
    assertEquals(
        apnDetailDialog.i18n('add'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn')
            .innerText);

    // Add a network.
    const network = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, apnDetailDialog.guid);
    mojoApi_.setManagedPropertiesForTest(network);
    await flushTasks();

    /**
     * @type {!ManagedProperties}
     */
    const properties =
        await mojoApi_.getManagedProperties(apnDetailDialog.guid);
    assertTrue(!!properties);
    assertFalse(!!properties.result.typeProperties.cellular.customApnList);

    const actionBtn =
        apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn');
    assertTrue(!!actionBtn);
    actionBtn.click();
    await flushTasks();
    await mojoApi_.whenCalled('createCustomApn');

    assertEquals(
        1, properties.result.typeProperties.cellular.customApnList.length);

    const apn = properties.result.typeProperties.cellular.customApnList[0];
    assertEquals(TEST_APN.accessPointName, apn.accessPointName);
    assertEquals(TEST_APN.username, apn.username);
    assertEquals(TEST_APN.password, apn.password);
    assertEquals(TEST_APN.authentication, apn.authentication);
    assertEquals(TEST_APN.ipType, apn.ipType);
    assertEquals(TEST_APN.apnTypes.length, apn.apnTypes.length);
    assertEquals(TEST_APN.apnTypes[0], apn.apnTypes[0]);
  });

  test('Setting mode to view changes buttons and fields', async () => {
    const assertFieldDisabled = (selector) => {
      const element = apnDetailDialog.shadowRoot.querySelector(selector);
      assertTrue(!!element);
      assertTrue(element.disabled);
    };

    // Set the dialog mode before opening the dialog so that the default focus
    // can be tested.
    apnDetailDialog.mode = ApnDetailDialogMode.VIEW;
    apnDetailDialog.apnProperties = TEST_APN;

    await init();
    assertEquals(
        apnDetailDialog.i18n('apnDetailViewApnDialogTitle'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailDialogTitle')
            .innerText);
    assertFalse(
        !!apnDetailDialog.shadowRoot.querySelector('#apnDetailCancelBtn'));
    assertFalse(
        !!apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn'));
    const doneBtn = apnDetailDialog.shadowRoot.querySelector('#apnDoneBtn');
    assertTrue(!!doneBtn);
    assertFalse(doneBtn.disabled);
    assertFieldDisabled('#apnInput');
    assertFieldDisabled('#usernameInput');
    assertFieldDisabled('#passwordInput');
    assertFieldDisabled('#authTypeDropDown');
    assertFieldDisabled('#apnDefaultTypeCheckbox');
    assertFieldDisabled('#apnAttachTypeCheckbox');
    assertFieldDisabled('#ipTypeDropDown');
    assertEquals(doneBtn, apnDetailDialog.shadowRoot.activeElement);
  });

  test('Dialog input fields are validated', async () => {
    await init();
    const apnInputField = apnDetailDialog.$.apnInput;
    const actionButton =
        apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn');
    assertTrue(!!actionButton);
    // Case: After opening dialog before user input
    assertFalse(!!apnInputField.invalid);
    assertTrue(!!actionButton.disabled);

    // Case : After valid user input
    apnInputField.value = 'test';
    assertFalse(!!apnInputField.invalid);
    assertFalse(!!actionButton.disabled);

    // Case : After Removing all user input no error state but button disabled
    apnInputField.value = '';
    assertFalse(!!apnInputField.invalid);
    assertTrue(!!actionButton.disabled);

    // Case : Non ascii user input
    apnInputField.value = 'testμ';
    assertTrue(!!apnInputField.invalid);
    assertTrue(!!actionButton.disabled);
    assertTrue(apnInputField.value.includes('μ'));

    // Case : longer than 63 characters then removing one character
    apnInputField.value = 'a'.repeat(64);
    assertTrue(!!apnInputField.invalid);
    assertTrue(!!actionButton.disabled);
    assertFalse(apnInputField.value.length > 63);
    apnInputField.value = apnInputField.value.slice(0, -1);
    assertFalse(!!apnInputField.invalid);
    assertFalse(!!actionButton.disabled);

    // Case : longer than 63 non-ASCII characters
    apnInputField.value = 'μ'.repeat(64);
    assertTrue(!!apnInputField.invalid);
    assertTrue(!!actionButton.disabled);
  });

  test('Apn types are correctly validated in all modes', async () => {
    await init();
    const updateApnTypeCheckboxes = (defaultType, attachType) => {
      apnDetailDialog.$.apnDefaultTypeCheckbox.checked = defaultType;
      apnDetailDialog.$.apnAttachTypeCheckbox.checked = attachType;
    };
    await toggleAdvancedSettings();
    const getDefaultApnInfo = () => {
      return apnDetailDialog.shadowRoot.querySelector(
          '#defaultApnRequiredInfo');
    };

    TEST_APN.id = '1';
    const currentApn = /** @type {ApnProperties} */ ({});
    Object.assign(currentApn, TEST_APN);
    currentApn.id = '2';
    apnDetailDialog.apnList = [TEST_APN, currentApn];
    apnDetailDialog.apnProperties = currentApn;

    const actionButton =
        apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn');
    apnDetailDialog.$.apnInput.value = 'valid_name';

    // CREATE mode tests
    apnDetailDialog.mode = ApnDetailDialogMode.CREATE;
    TEST_APN.state = ApnState.kDisabled;
    apnDetailDialog.apnList = [TEST_APN];

    // Case: Default APN type is checked
    updateApnTypeCheckboxes(/* default= */ true, /* attach= */ false);
    await flushTasks();
    assertFalse(actionButton.disabled);
    assertFalse(!!getDefaultApnInfo());

    // Case: No enabled default APNs, default unchecked and attach is checked.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ true);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertTrue(!!getDefaultApnInfo());

    // Case: No enabled default APNs and both unchecked.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertFalse(!!getDefaultApnInfo());

    // Case: Enabled default APNs, default unchecked and attach is checked.
    TEST_APN.state = ApnState.kEnabled;
    apnDetailDialog.apnList = [TEST_APN];
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ true);
    await flushTasks();
    assertFalse(actionButton.disabled);
    assertFalse(!!getDefaultApnInfo());

    // Case: Enabled default APNs and both unchecked.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertFalse(!!getDefaultApnInfo());

    // Edit mode tests
    apnDetailDialog.mode = ApnDetailDialogMode.EDIT;
    TEST_APN.apnTypes = [ApnType.kAttach];
    currentApn.apnTypes = [ApnType.kDefault, ApnType.kAttach];
    apnDetailDialog.apnList = [TEST_APN, currentApn];

    // Case: Default APN type is checked
    updateApnTypeCheckboxes(/* default= */ true, /* attach= */ false);
    await flushTasks();
    assertFalse(actionButton.disabled);
    assertFalse(!!getDefaultApnInfo());

    // Case: User unchecks the default checkbox, APN being modified is the
    // only default APN
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ true);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertTrue(!!getDefaultApnInfo());

    // Case: User unchecks both checkboxes, APN being modified is the
    // only enabled default APN but there are other enabled attach APNs.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertTrue(!!getDefaultApnInfo());

    // Case: User unchecks both checkboxes, APN being modified is the
    // only enabled default APN and is the only enabled attachApn.
    currentApn.apnTypes = [ApnType.kDefault, ApnType.kAttach];
    apnDetailDialog.apnList = [currentApn];
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertFalse(!!getDefaultApnInfo());

    // Case: User unchecks default APN type checkbox and checks the attach
    // APN type checkbox, APN being modified is the only enabled default APN
    // and there are no other enabled attach type APNs.
    currentApn.apnTypes = [ApnType.kDefault];
    apnDetailDialog.apnList = [currentApn];
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ true);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertTrue(!!getDefaultApnInfo());
  });

  test('Setting mode to edit changes buttons and fields', async () => {
    const apnWithId = TEST_APN;
    apnWithId.id = '1';
    apnWithId.apnTypes = [ApnType.kDefault];

    // Set the dialog mode before opening the dialog so that the default focus
    // can be tested.
    apnDetailDialog.mode = ApnDetailDialogMode.EDIT;
    apnDetailDialog.apnProperties = apnWithId;

    await init();
    assertEquals(
        apnDetailDialog.i18n('apnDetailEditApnDialogTitle'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailDialogTitle')
            .innerText);
    assertElementEnabled('#apnDetailCancelBtn');
    assertElementEnabled('#apnDetailActionBtn');
    assertEquals(
        apnDetailDialog.i18n('save'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn')
            .innerText);
    assertFalse(!!apnDetailDialog.shadowRoot.querySelector('#apnDoneBtn'));
    assertAllInputsEnabled();

    // Case: clicking on the action button calls the correct method
    const network = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, apnDetailDialog.guid);
    mojoApi_.setManagedPropertiesForTest(network);
    await flushTasks();
    const managedProperties =
        await mojoApi_.getManagedProperties(apnDetailDialog.guid);
    assertTrue(!!managedProperties);
    managedProperties.result.typeProperties.cellular = {
      customApnList: [apnWithId],
    };
    const newExpectedApn = 'modified';
    apnDetailDialog.$.apnInput.value = newExpectedApn;
    apnDetailDialog.shadowRoot.querySelector('#apnDetailActionBtn').click();
    await mojoApi_.whenCalled('modifyCustomApn');

    const apn =
        managedProperties.result.typeProperties.cellular.customApnList[0];
    assertEquals(newExpectedApn, apn.accessPointName);
    assertEquals(apnWithId.id, apn.id);
    assertEquals(apnWithId.username, apn.username);
    assertEquals(apnWithId.password, apn.password);
    assertEquals(apnWithId.authentication, apn.authentication);
    assertEquals(apnWithId.ipType, apn.ipType);
    assertEquals(apnWithId.apnTypes.length, apn.apnTypes.length);
    assertEquals(apnWithId.apnTypes[0], apn.apnTypes[0]);
    assertEquals(
        apnDetailDialog.shadowRoot.querySelector('#apnInput'),
        apnDetailDialog.shadowRoot.activeElement);
  });

});
