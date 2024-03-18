// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {ApnDetailDialog, CrCheckboxElement, CrDialogElement, CrInputElement} from 'chrome://os-settings/os_settings.js';
import {ApnDetailDialogMode} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnSource, ApnState, ApnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const TEST_APN: ApnProperties = {
  accessPointName: 'apn',
  username: 'username',
  password: 'password',
  authentication: ApnAuthenticationType.kAutomatic,
  ipType: ApnIpType.kAutomatic,
  apnTypes: [ApnType.kDefault],
  state: ApnState.kEnabled,
  id: undefined,
  language: undefined,
  localizedName: undefined,
  name: undefined,
  attach: undefined,
  source: ApnSource.kUi,
};

suite('<apn-detail-dialog>', () => {
  let apnDetailDialog: ApnDetailDialog;
  let mojoApi: FakeNetworkConfig;

  function toggleAdvancedSettings(): void {
    const advancedSettingsBtn =
        apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#advancedSettingsBtn');
    assertTrue(!!advancedSettingsBtn);
    advancedSettingsBtn.click();
  }

  function assertElementEnabled(selector: string): void {
    const element = apnDetailDialog.shadowRoot!.querySelector<
        HTMLInputElement|HTMLSelectElement|HTMLButtonElement|CrCheckboxElement>(
        selector);
    assertTrue(!!element);
    assertFalse(element.disabled);
  }

  function assertAllInputsEnabled(): void {
    assertElementEnabled('#apnInput');
    assertElementEnabled('#usernameInput');
    assertElementEnabled('#passwordInput');
    assertElementEnabled('#authTypeDropDown');
    assertElementEnabled('#apnDefaultTypeCheckbox');
    assertElementEnabled('#apnAttachTypeCheckbox');
    assertElementEnabled('#ipTypeDropDown');
  }

  suiteSetup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
  });

  teardown(() => {
    apnDetailDialog.remove();
    mojoApi.resetForTest();
  });

  async function init(
      mode?: ApnDetailDialogMode,
      apnProperties?: ApnProperties): Promise<void> {
    apnDetailDialog = document.createElement('apn-detail-dialog');
    apnDetailDialog.guid = 'fake-guid';
    apnDetailDialog.apnList = [TEST_APN];
    apnDetailDialog.mode = mode || ApnDetailDialogMode.CREATE;
    apnDetailDialog.apnProperties = apnProperties;
    document.body.appendChild(apnDetailDialog);
    await waitAfterNextRender(apnDetailDialog);
  }

  test('Element contains dialog', async () => {
    await init();
    const dialog = apnDetailDialog.shadowRoot!.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    // Confirm that the dialog has the add apn title.
    const apnDetailDialogTitle =
        apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
            '#apnDetailDialogTitle');
    assertTrue(!!apnDetailDialogTitle);
    assertEquals(
        apnDetailDialog.i18n('apnDetailAddApnDialogTitle'),
        apnDetailDialogTitle.innerText);
    assertEquals('polite', apnDetailDialogTitle.ariaLive);
    assertTrue(!!apnDetailDialog.shadowRoot!.querySelector('#apnInput'));
    assertTrue(!!apnDetailDialog.shadowRoot!.querySelector('#usernameInput'));
    assertTrue(!!apnDetailDialog.shadowRoot!.querySelector('#passwordInput'));

    assertTrue(
        !!apnDetailDialog.shadowRoot!.querySelector('#authTypeDropDown'));
    const defaultTypeCheckbox =
        apnDetailDialog.shadowRoot!.querySelector<CrCheckboxElement>(
            '#apnDefaultTypeCheckbox');
    assertTrue(!!defaultTypeCheckbox);
    assertTrue(defaultTypeCheckbox.checked);
    assertTrue(
        !!apnDetailDialog.shadowRoot!.querySelector('#apnAttachTypeCheckbox'));
    assertTrue(!!apnDetailDialog.shadowRoot!.querySelector('#ipTypeDropDown'));
    assertTrue(
        !!apnDetailDialog.shadowRoot!.querySelector('#apnDetailCancelBtn'));
    assertTrue(
        !!apnDetailDialog.shadowRoot!.querySelector('#apnDetailActionBtn'));
    assertNull(apnDetailDialog.shadowRoot!.querySelector('#apnDoneBtn'));
    assertEquals(
        apnDetailDialog.shadowRoot!.querySelector('#apnInput'),
        apnDetailDialog.shadowRoot!.activeElement);
  });

  test(
      'Add button becoming enabled and disabled uses correct a11y text',
      async () => {
        await init();

        let actionButtonEnabledA11yText =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#actionButtonEnabledA11yText');

        // Button state should not be announced when dialog opens initially.
        // Announcement should only be made when the enabled state changes
        // from disabled to enabled.
        assertFalse(!!actionButtonEnabledA11yText);

        const apnInput =
            apnDetailDialog.shadowRoot!.querySelector<CrInputElement>(
                '#apnInput');
        assertTrue(!!apnInput);
        apnInput.value = TEST_APN.accessPointName;
        await flushTasks();

        // Button state becomes enabled, announcement should be made.
        actionButtonEnabledA11yText =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#actionButtonEnabledA11yText');
        assertTrue(!!actionButtonEnabledA11yText);
        assertEquals(
            apnDetailDialog.i18n('apnDetailDialogA11yAddEnabled'),
            actionButtonEnabledA11yText.innerText);

        apnInput.value = '';
        await flushTasks();

        // Button state becomes disabled, announcement should be made.
        actionButtonEnabledA11yText =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#actionButtonEnabledA11yText');
        assertTrue(!!actionButtonEnabledA11yText);
        assertEquals(
            apnDetailDialog.i18n('apnDetailDialogA11yAddDisabled'),
            actionButtonEnabledA11yText.innerText);
      });

  test(
      'Save button becoming disabled and enabled uses correct a11y text',
      async () => {
        const apnWithId = TEST_APN;
        apnWithId.id = '1';
        apnWithId.apnTypes = [ApnType.kDefault];

        await init(
            /* mode= */ ApnDetailDialogMode.EDIT,
            /* apnProperties= */ apnWithId);

        let actionButtonEnabledA11yText =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#actionButtonEnabledA11yText');

        // Button state should not be announced when dialog opens initially.
        // Announcement should only be made when the enabled state changes
        // from enabled to disabled.
        assertFalse(!!actionButtonEnabledA11yText);

        const apnInput =
            apnDetailDialog.shadowRoot!.querySelector<CrInputElement>(
                '#apnInput');
        assertTrue(!!apnInput);

        apnInput.value = '';
        await flushTasks();

        // Button state becomes disabled, announcement should be made.
        actionButtonEnabledA11yText =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#actionButtonEnabledA11yText');
        assertTrue(!!actionButtonEnabledA11yText);

        assertEquals(
            apnDetailDialog.i18n('apnDetailDialogA11ySaveDisabled'),
            actionButtonEnabledA11yText.innerText);

        apnInput.value = 'new.apn';
        await flushTasks();

        // Button state becomes enabled, announcement should be made.
        actionButtonEnabledA11yText =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#actionButtonEnabledA11yText');
        assertTrue(!!actionButtonEnabledA11yText);

        assertEquals(
            apnDetailDialog.i18n('apnDetailDialogA11ySaveEnabled'),
            actionButtonEnabledA11yText.innerText);
      });

  test('Clicking the cancel button fires the close event', async () => {
    await init();
    const closeEventPromise = eventToPromise('close', window);
    const cancelBtn =
        apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#apnDetailCancelBtn');
    assertTrue(!!cancelBtn);

    cancelBtn.click();
    await closeEventPromise;
    const crDialogElement =
        apnDetailDialog.shadowRoot!.querySelector<CrDialogElement>(
            '#apnDetailDialog');
    assertTrue(!!crDialogElement);
    assertFalse(crDialogElement.open);
  });

  test(
      'Clicking on the advanced settings button expands/collapses section',
      async () => {
        await init();
        assertEquals(
            'apnDetailDialogTitle',
            apnDetailDialog.shadowRoot!.querySelector('#advancedSettingsBtn')!
                .getAttribute('aria-describedby'));
        const isAdvancedSettingShowing = () => {
          const ironCollapseElement =
              apnDetailDialog.shadowRoot!.querySelector('iron-collapse');
          assertTrue(!!ironCollapseElement);
          return ironCollapseElement.opened;
        };
        assertFalse(isAdvancedSettingShowing());
        toggleAdvancedSettings();
        assertTrue(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        assertFalse(isAdvancedSettingShowing());
        toggleAdvancedSettings();
        const assertOptions =
            (expectedTextArray: string[],
             optionNodes: NodeListOf<HTMLOptionElement>) => {
              for (const [idx, expectedText] of expectedTextArray.entries()) {
                assertTrue(!!optionNodes[idx]);
                assertTrue(!!optionNodes[idx]!.text);
                assertEquals(expectedText, optionNodes[idx]!.text);
              }
            };
        const authTypeDropDown =
            apnDetailDialog.shadowRoot!.querySelector('#authTypeDropDown');
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
            apnDetailDialog.shadowRoot!.querySelector('#ipTypeDropDown');
        assertTrue(!!ipTypeDropDown);
        const ipTypeOptionNodes =
            ipTypeDropDown.querySelectorAll<HTMLOptionElement>('option');
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
    const apnInput =
        apnDetailDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#apnInput');
    assertTrue(!!apnInput);
    apnInput.value = TEST_APN.accessPointName;
    const usernameInput =
        apnDetailDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#usernameInput');
    assertTrue(!!usernameInput);
    usernameInput.value = TEST_APN.username!;
    const passwordInput =
        apnDetailDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#passwordInput');
    assertTrue(!!passwordInput);
    passwordInput.value = TEST_APN.password!;

    assertAllInputsEnabled();
    assertElementEnabled('#apnDetailCancelBtn');
    let actionBtn =
        apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#apnDetailActionBtn');
    assertTrue(!!actionBtn);
    assertEquals(apnDetailDialog.i18n('add'), actionBtn.innerText);

    // Add a network.
    const network = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, apnDetailDialog.guid, apnDetailDialog.guid);
    mojoApi.setManagedPropertiesForTest(network);
    await flushTasks();

    const properties = await mojoApi.getManagedProperties(apnDetailDialog.guid);
    assertTrue(!!properties);
    assertEquals(
        undefined, properties.result.typeProperties.cellular!.customApnList);

    actionBtn = apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#apnDetailActionBtn');
    assertTrue(!!actionBtn);
    actionBtn.click();
    await flushTasks();
    await mojoApi.whenCalled('createCustomApn');

    assertEquals(
        1, properties.result.typeProperties.cellular!.customApnList!.length);

    const apn = properties.result.typeProperties.cellular!.customApnList![0];
    assertTrue(!!apn);
    assertEquals(TEST_APN.accessPointName, apn.accessPointName);
    assertEquals(TEST_APN.username, apn.username);
    assertEquals(TEST_APN.password, apn.password);
    assertEquals(TEST_APN.authentication, apn.authentication);
    assertEquals(TEST_APN.ipType, apn.ipType);
    assertEquals(TEST_APN.apnTypes.length, apn.apnTypes.length);
    assertEquals(TEST_APN.apnTypes[0], apn.apnTypes[0]);
  });

  test('Setting mode to view changes buttons and fields', async () => {
    const assertFieldDisabled = (selector: string) => {
      const element = apnDetailDialog.shadowRoot!.querySelector<
          HTMLInputElement|HTMLSelectElement|CrCheckboxElement>(selector);
      assertTrue(!!element);
      assertTrue(element.disabled);
    };

    // Set the dialog mode before opening the dialog so that the default focus
    // can be tested.
    await init(
        /* mode= */ ApnDetailDialogMode.VIEW, /* apnProperties= */ TEST_APN);

    const apnDetailDialogTitle =
        apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
            '#apnDetailDialogTitle');
    assertTrue(!!apnDetailDialogTitle);
    assertEquals(
        apnDetailDialog.i18n('apnDetailViewApnDialogTitle'),
        apnDetailDialogTitle.innerText);
    assertNull(
        apnDetailDialog.shadowRoot!.querySelector('#apnDetailCancelBtn'));
    assertNull(
        apnDetailDialog.shadowRoot!.querySelector('#apnDetailActionBtn'));
    const doneBtn =
        apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#apnDoneBtn');
    assertTrue(!!doneBtn);
    assertFalse(doneBtn.disabled);
    assertFieldDisabled('#apnInput');
    assertFieldDisabled('#usernameInput');
    assertFieldDisabled('#passwordInput');
    assertFieldDisabled('#authTypeDropDown');
    assertFieldDisabled('#apnDefaultTypeCheckbox');
    assertFieldDisabled('#apnAttachTypeCheckbox');
    assertFieldDisabled('#ipTypeDropDown');
    assertEquals(doneBtn, apnDetailDialog.shadowRoot!.activeElement);
  });

  test('Dialog input fields are validated', async () => {
    await init();
    const apnInputField =
        apnDetailDialog.shadowRoot!.querySelector<CrInputElement>('#apnInput');
    assertTrue(!!apnInputField);
    const actionButton =
        apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#apnDetailActionBtn');
    assertTrue(!!actionButton);
    // Case: After opening dialog before user input
    assertFalse(apnInputField.invalid);
    assertTrue(actionButton.disabled);

    // Case : After valid user input
    apnInputField.value = 'test';
    assertFalse(apnInputField.invalid);
    assertFalse(actionButton.disabled);

    // Case : After Removing all user input no error state but button disabled
    apnInputField.value = '';
    assertFalse(apnInputField.invalid);
    assertTrue(actionButton.disabled);

    // Case : Non ascii user input
    apnInputField.value = 'testμ';
    assertTrue(apnInputField.invalid);
    assertTrue(actionButton.disabled);
    assertStringContains(apnInputField.value, 'μ');
    assertEquals(
        apnDetailDialog.i18n('apnDetailApnErrorInvalidChar'),
        apnInputField.errorMessage);

    // Case : longer than 63 characters then removing one character
    apnInputField.value = 'a'.repeat(64);
    assertTrue(apnInputField.invalid);
    assertTrue(actionButton.disabled);
    assertEquals(63, apnInputField.value.length);
    assertEquals(
        apnDetailDialog.i18n('apnDetailApnErrorMaxChars', 63),
        apnInputField.errorMessage);
    apnInputField.value = apnInputField.value.slice(0, -1);
    assertFalse(apnInputField.invalid);
    assertFalse(actionButton.disabled);

    // Case : longer than 63 non-ASCII characters
    apnInputField.value = 'μ'.repeat(64);
    assertTrue(apnInputField.invalid);
    assertTrue(actionButton.disabled);
    assertEquals(
        apnDetailDialog.i18n('apnDetailApnErrorMaxChars', 63),
        apnInputField.errorMessage);
  });

  test('Apn types are correctly validated in all modes', async () => {
    await init();
    const updateApnTypeCheckboxes =
        (defaultType: boolean, attachType: boolean) => {
          const apnDefaultTypeCheckbox =
              apnDetailDialog.shadowRoot!.querySelector<CrCheckboxElement>(
                  '#apnDefaultTypeCheckbox');
          assertTrue(!!apnDefaultTypeCheckbox);
          apnDefaultTypeCheckbox.checked = defaultType;

          assertEquals(
              'apnDetailApnTypesLabel',
              apnDefaultTypeCheckbox.getAttribute('aria-describedby'));

          const apnAttachTypeCheckbox =
              apnDetailDialog.shadowRoot!.querySelector<CrCheckboxElement>(
                  '#apnAttachTypeCheckbox');
          assertTrue(!!apnAttachTypeCheckbox);
          assertEquals(
              'apnDetailApnTypesLabel',
              apnAttachTypeCheckbox.getAttribute('aria-describedby'));

          apnAttachTypeCheckbox.checked = attachType;
        };

    toggleAdvancedSettings();
    const getDefaultApnInfo = () =>
        apnDetailDialog.shadowRoot!.querySelector('#defaultApnRequiredInfo');

    TEST_APN.id = '1';
    const currentApn = {...TEST_APN};
    currentApn.id = '2';
    apnDetailDialog.set('apnList', [TEST_APN, currentApn]);
    apnDetailDialog.set('apnProperties', currentApn);

    const actionButton =
        apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#apnDetailActionBtn');
    assertTrue(!!actionButton);
    const apnInputField =
        apnDetailDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#apnInput');
    assertTrue(!!apnInputField);
    apnInputField.value = 'valid_name';

    // CREATE mode tests
    apnDetailDialog.mode = ApnDetailDialogMode.CREATE;
    TEST_APN.state = ApnState.kDisabled;
    apnDetailDialog.set('apnList', [TEST_APN]);

    // Case: Default APN type is checked
    updateApnTypeCheckboxes(/* default= */ true, /* attach= */ false);
    await flushTasks();
    assertFalse(actionButton.disabled);
    assertNull(getDefaultApnInfo());

    // Case: No enabled default APNs, default unchecked and attach is checked.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ true);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertTrue(!!getDefaultApnInfo());

    // Case: No enabled default APNs and both unchecked.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertNull(getDefaultApnInfo());

    // Case: Enabled default APNs, default unchecked and attach is checked.
    TEST_APN.state = ApnState.kEnabled;
    apnDetailDialog.set('apnList', [TEST_APN]);
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ true);
    await flushTasks();
    assertFalse(actionButton.disabled);
    assertNull(getDefaultApnInfo());

    // Case: Enabled default APNs and both unchecked.
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertNull(getDefaultApnInfo());

    // Edit mode tests
    apnDetailDialog.set('mode', ApnDetailDialogMode.EDIT);
    TEST_APN.apnTypes = [ApnType.kAttach];
    currentApn.apnTypes = [ApnType.kDefault, ApnType.kAttach];
    apnDetailDialog.set('apnList', [TEST_APN, currentApn]);

    // Case: Default APN type is checked
    updateApnTypeCheckboxes(/* default= */ true, /* attach= */ false);
    await flushTasks();
    assertFalse(actionButton.disabled);
    assertNull(getDefaultApnInfo());

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
    apnDetailDialog.set('apnList', [currentApn]);
    updateApnTypeCheckboxes(/* default= */ false, /* attach= */ false);
    await flushTasks();
    assertTrue(actionButton.disabled);
    assertNull(getDefaultApnInfo());

    // Case: User unchecks default APN type checkbox and checks the attach
    // APN type checkbox, APN being modified is the only enabled default APN
    // and there are no other enabled attach type APNs.
    currentApn.apnTypes = [ApnType.kDefault];
    apnDetailDialog.set('apnList', [currentApn]);
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
    await init(
        /* mode= */ ApnDetailDialogMode.EDIT, /* apnProperties= */ apnWithId);

    const apnDetailDialogTitle =
        apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
            '#apnDetailDialogTitle');
    assertTrue(!!apnDetailDialogTitle);
    assertEquals(
        apnDetailDialog.i18n('apnDetailEditApnDialogTitle'),
        apnDetailDialogTitle.innerText);
    assertElementEnabled('#apnDetailCancelBtn');
    let button = apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#apnDetailActionBtn');
    assertTrue(!!button);
    assertEquals(apnDetailDialog.i18n('save'), button.innerText);
    assertNull(apnDetailDialog.shadowRoot!.querySelector('#apnDoneBtn'));
    assertAllInputsEnabled();

    // Case: clicking on the action button calls the correct method
    const network = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, apnDetailDialog.guid, apnDetailDialog.guid);
    mojoApi.setManagedPropertiesForTest(network);
    await flushTasks();
    const managedProperties =
        await mojoApi.getManagedProperties(apnDetailDialog.guid);
    assertTrue(!!managedProperties);
    mojoApi.createCustomApn(apnDetailDialog.guid, apnWithId);
    const newExpectedApn = 'modified';
    const apnInputField =
        apnDetailDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#apnInput');
    assertTrue(!!apnInputField);
    apnInputField.value = newExpectedApn;
    button = apnDetailDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#apnDetailActionBtn');
    assertTrue(!!button);
    button.click();
    await mojoApi.whenCalled('modifyCustomApn');

    const apn =
        managedProperties.result.typeProperties.cellular!.customApnList![0];
    assertTrue(!!apn);
    assertEquals(newExpectedApn, apn.accessPointName);
    assertEquals(apnWithId.id, apn.id);
    assertEquals(apnWithId.username, apn.username);
    assertEquals(apnWithId.password, apn.password);
    assertEquals(apnWithId.authentication, apn.authentication);
    assertEquals(apnWithId.ipType, apn.ipType);
    assertEquals(apnWithId.apnTypes.length, apn.apnTypes.length);
    assertEquals(apnWithId.apnTypes[0], apn.apnTypes[0]);
    assertEquals(
        apnDetailDialog.shadowRoot!.querySelector('#apnInput'),
        apnDetailDialog.shadowRoot!.activeElement);
  });
});
