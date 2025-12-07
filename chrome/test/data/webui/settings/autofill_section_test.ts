// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrInputElement, CrTextareaElement} from 'chrome://settings/lazy_load.js';
import {AutofillAddressOptInChange, AutofillManagerImpl, CountryDetailManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {CrLinkRowElement} from 'chrome://settings/settings.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {eventToPromise, whenAttributeIs, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {AutofillManagerExpectations, createAddressEntry, createEmptyAddressEntry, STUB_USER_ACCOUNT_INFO, TestAutofillManager} from './autofill_fake_data.js';
import {createAutofillSection, initiateRemoving, initiateEditing, createAddressDialog, createRemoveAddressDialog, expectEvent, openAddressDialog, getAddressFieldValue} from './autofill_section_test_utils.js';
import {TestCountryDetailManagerProxy} from './test_country_detail_manager_proxy.js';
// clang-format on

const FieldType = chrome.autofillPrivate.FieldType;

const ADDRESS_COMPONENTS_US = {
  components: [
    {
      row: [
        {
          field: FieldType.NAME_FULL,
          fieldName: 'Name',
          isLongField: true,
          isRequired: false,
        },
      ],
    },
    {
      row: [
        {
          field: FieldType.ADDRESS_HOME_CITY,
          fieldName: 'City',
          isLongField: false,
          isRequired: true,
        },
        {
          field: FieldType.ADDRESS_HOME_STATE,
          fieldName: 'State',
          isLongField: false,
          isRequired: true,
        },
        {
          field: FieldType.ADDRESS_HOME_ZIP,
          fieldName: 'ZIP code',
          isLongField: false,
          isRequired: true,
        },
      ],
    },
  ],
  languageCode: 'en',
};

const ADDRESS_COMPONENTS_GB = {
  components: [
    {
      row: [
        {
          field: FieldType.NAME_FULL,
          fieldName: 'Name',
          isLongField: true,
          isRequired: false,
        },
      ],
    },
    {
      row: [
        {
          field: FieldType.ADDRESS_HOME_CITY,
          fieldName: 'Post town',
          isLongField: false,
          isRequired: true,
        },
      ],
    },
    {
      row: [
        {
          field: FieldType.ADDRESS_HOME_ZIP,
          fieldName: 'Postal code',
          isLongField: false,
          isRequired: true,
        },
      ],
    },
    {
      row: [
        {
          field: FieldType.ADDRESS_HOME_STATE,
          fieldName: 'County',
          isLongField: false,
          isRequired: true,
        },
      ],
    },
  ],
  languageCode: 'en',
};

const ADDRESS_COMPONENTS_IL = {
  components: [
    {
      row: [
        {
          field: FieldType.NAME_FULL,
          fieldName: 'Name',
          isLongField: true,
          isRequired: false,
        },
      ],
    },
    {
      row: [
        {
          field: FieldType.ADDRESS_HOME_CITY,
          fieldName: 'City',
          isLongField: false,
          isRequired: true,
        },
        {
          field: FieldType.ADDRESS_HOME_ZIP,
          fieldName: 'Postal code',
          isLongField: false,
          isRequired: true,
        },
      ],
    },
  ],
  languageCode: 'iw',
};

suite('AutofillSectionUiTest', function() {
  test('AutofillExtensionIndicator', function() {
    // Initializing with fake prefs
    const section = document.createElement('settings-autofill-section');
    section.prefs = {autofill: {profile_enabled: {}}};
    document.body.appendChild(section);

    assertFalse(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
    section.set('prefs.autofill.profile_enabled.extensionId', 'test-id');
    flush();

    assertTrue(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));

    document.body.removeChild(section);
  });

  test('verifyAddressDeleteRecordTypeNotice', async () => {
    const address = createAddressEntry();
    const accountAddress = createAddressEntry();
    accountAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT;

    const autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [address, accountAddress];
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');

    await flushTasks();

    {
      const dialog = await initiateRemoving(section, 0);
      const expectedMessage =
          loadTimeData.getString('removeSyncAddressConfirmationDescription');
      assertEquals(
          dialog.$.description.textContent.trim(), expectedMessage,
          'Sync-on message should be visible');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    const changeListener =
        autofillManager.lastCallback.setPersonalDataManagerListener;
    assertTrue(
        !!changeListener,
        'PersonalDataChangedListener should be set in the section element');

    // Imitate disabling sync.
    changeListener(autofillManager.data.addresses, [], [], [], {
      ...STUB_USER_ACCOUNT_INFO,
    });

    {
      const dialog = await initiateRemoving(section, 0);
      const expectedMessage =
          loadTimeData.getString('removeLocalAddressConfirmationDescription');
      assertEquals(
          dialog.$.description.textContent.trim(), expectedMessage,
          'Sync-off message should be visible');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    // Imitate disabling sync.
    changeListener(autofillManager.data.addresses, [], [], [], undefined);

    {
      const dialog = await initiateRemoving(section, 0);
      const expectedMessage =
          loadTimeData.getString('removeLocalAddressConfirmationDescription');
      assertEquals(
          dialog.$.description.textContent.trim(), expectedMessage,
          'Sync-off message should be visible when account info is missing');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    changeListener(autofillManager.data.addresses, [], [], [], {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    });

    {
      const dialog = await initiateRemoving(section, 1);
      const expectedMessage = loadTimeData.getStringF(
          'deleteAccountAddressRecordTypeNotice', STUB_USER_ACCOUNT_INFO.email);
      assertEquals(
          dialog.$.description.textContent.trim(), expectedMessage,
          'Account address message should be visible');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });

  test('verifyAddressDeleteHomeAddressNotice', async () => {
    const homeAddress = createAddressEntry();
    homeAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_HOME;

    const autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [homeAddress];
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    {
      const dialog = await initiateRemoving(section, 0);
      const homeUrl = loadTimeData.getString('googleAccountHomeAddressUrl')
                          .replace(/&/g, '&amp;');
      const expectedMessage = loadTimeData.getStringF(
          'deleteHomeAddressNotice', homeUrl, STUB_USER_ACCOUNT_INFO.email);
      assertEquals(
          dialog.$.description.innerHTML, expectedMessage,
          `Home address delete confirmation view description is incorrect.`);
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });

  test('verifyAddressDeleteWorkAddressNotice', async () => {
    const workAddress = createAddressEntry();
    workAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_WORK;

    const autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [workAddress];
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    {
      const dialog = await initiateRemoving(section, 0);
      const workUrl = loadTimeData.getString('googleAccountWorkAddressUrl')
                          .replace(/&/g, '&amp;');
      const expectedMessage = loadTimeData.getStringF(
          'deleteWorkAddressNotice', workUrl, STUB_USER_ACCOUNT_INFO.email);
      assertEquals(
          dialog.$.description.innerHTML, expectedMessage,
          `Work address delete confirmation view description is incorrect.`);
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });

  test('verifyAddressDeleteNameEmailAddressNotice', async () => {
    const nameEmailAddress = createAddressEntry();
    nameEmailAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL;

    const autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [nameEmailAddress];
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    {
      const dialog = await initiateRemoving(section, 0);
      const nameEmailUrl =
          loadTimeData.getString('googleAccountNameEmailAddressEditUrl')
              .replace(/&/g, '&amp;');
      const expectedDescription = loadTimeData.getStringF(
          'deleteNameEmailAddressNotice', nameEmailUrl,
          STUB_USER_ACCOUNT_INFO.email);
      assertEquals(
          dialog.$.description.innerHTML, expectedDescription,
          `Name email delete confirmation view description is incorrect.`);

      const title = dialog.shadowRoot!.querySelector<HTMLElement>('#title');
      assertTrue(!!title);
      assertEquals(
          title.innerHTML,
          loadTimeData.getString('removeNameEmailAddressConfirmationTitle'),
          `Name email delete confirmation view title is incorrect.`);

      const removeButton =
          dialog.shadowRoot!.querySelector<HTMLElement>('#remove');
      assertTrue(!!removeButton);
      assertEquals(
          removeButton.innerText,
          loadTimeData.getString('removeAddressFromChrome'),
          `Name email delete confirmation remove button label is incorrect.`);
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });

  test('verifyAddressEditRecordTypeNotice', async () => {
    const email = 'stub-user@example.com';
    const address = createAddressEntry();
    const accountAddress = createAddressEntry();
    accountAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT;
    const section = await createAutofillSection([address, accountAddress], {}, {
      ...STUB_USER_ACCOUNT_INFO,
      email,
    });

    {
      const dialog = await initiateEditing(section, 0);
      assertFalse(
          isVisible(dialog.$.accountRecordTypeNotice),
          'account notice should be invisible for non-account address');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    {
      const dialog = await initiateEditing(section, 1);
      assertTrue(
          isVisible(dialog.$.accountRecordTypeNotice),
          'account notice should be visible for account address');

      assertEquals(
          dialog.$.accountRecordTypeNotice.innerText,
          section.i18n('editAccountAddressRecordTypeNotice', email));

      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });
});

suite('AutofillSectionAddressTests', function() {
  let countryDetailManager: TestCountryDetailManagerProxy;
  let metricsTracker: MetricsTracker;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metricsTracker = fakeMetricsPrivate();

    countryDetailManager = new TestCountryDetailManagerProxy();
    CountryDetailManagerProxyImpl.setInstance(countryDetailManager);

    countryDetailManager.setGetCountryListRepsonse([
      {name: 'United States', countryCode: 'US'},  // Default country.
      {name: 'Israel', countryCode: 'IL'},
      {name: 'United Kingdom', countryCode: 'GB'},
    ]);
    countryDetailManager.setGetAddressFormatRepsonse(ADDRESS_COMPONENTS_US);
  });

  test('verifyAutofillAddressToggleMetric', async function() {
    const section =
        await createAutofillSection([], {profile_enabled: {value: true}});
    const button = section.$.autofillProfileToggle;
    assertTrue(!!button);

    // The address profile toggle is on by default.
    assertTrue(button.checked);
    assertEquals(metricsTracker.count('Autofill.Address.IsEnabled.Change'), 0);

    // Test that toggling the button off records the correct metric.
    button.click();
    assertEquals(metricsTracker.count('Autofill.Address.IsEnabled.Change'), 1);
    assertEquals(
        metricsTracker.count(
            'Autofill.Address.IsEnabled.Change',
            AutofillAddressOptInChange.OPT_OUT),
        1);

    // Test that toggling the button on records the correct metric.
    button.click();
    assertEquals(metricsTracker.count('Autofill.Address.IsEnabled.Change'), 2);
    assertEquals(
        metricsTracker.count(
            'Autofill.Address.IsEnabled.Change',
            AutofillAddressOptInChange.OPT_IN),
        1);
  });

  test('verifyNoAddresses', async function() {
    const section =
        await createAutofillSection([], {profile_enabled: {value: true}});

    const addressList = section.$.addressList;
    assertTrue(!!addressList);
    // 1 for the template element.
    assertEquals(1, addressList.children.length);

    assertFalse(section.$.noAddressesLabel.hidden);
    assertFalse(section.$.addAddress.disabled);
    assertFalse(section.$.autofillProfileToggle.disabled);
  });

  test('verifyAddressCount', async function() {
    const addresses = [
      createAddressEntry(),
      createAddressEntry(),
      createAddressEntry(),
      createAddressEntry(),
      createAddressEntry(),
    ];

    const section = await createAutofillSection(
        addresses, {profile_enabled: {value: true}});

    const addressList = section.$.addressList;
    assertTrue(!!addressList);
    assertEquals(
        addresses.length, addressList.querySelectorAll('.list-item').length);

    assertTrue(section.$.noAddressesLabel.hidden);
    assertFalse(section.$.autofillProfileToggle.disabled);
    assertFalse(section.$.addAddress.disabled);
  });

  test('verifyAddressDisabled', async function() {
    const section =
        await createAutofillSection([], {profile_enabled: {value: false}});

    assertFalse(section.$.autofillProfileToggle.disabled);
    assertTrue(section.$.addAddress.hidden);
  });

  test('verifyAddressFields', async function() {
    const address = createAddressEntry();
    const section = await createAutofillSection([address], {});
    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);

    const addressSummary =
        address.metadata!.summaryLabel + address.metadata!.summarySublabel;

    let actualSummary = '';

    // Eliminate white space between nodes!
    const addressPieces = row.querySelector('#addressSummary')!.children;
    for (const addressPiece of addressPieces) {
      actualSummary += addressPiece.textContent.trim();
    }

    assertEquals(addressSummary, actualSummary);
  });

  test('verifyAddressLocalIndication', async () => {
    const autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [createAddressEntry()];
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    const addressList = section.$.addressList;
    const getIcon = () => addressList.children[0]!.querySelector<HTMLElement>(
        '#address-row-icon');

    const iconName1 = getIcon()!.getAttribute('icon');
    assertFalse(
        !!iconName1 && iconName1.includes('cloud-off'),
        'Sync for addresses is enabled, the local indicator should be off.');

    const changeListener =
        autofillManager.lastCallback.setPersonalDataManagerListener!;
    changeListener(autofillManager.data.addresses, [], [], [], undefined);
    const iconName2 = getIcon()!.getAttribute('icon');
    assertFalse(
        !!iconName2 && iconName2.includes('cloud-off'),
        'The local indicator should not be shown to logged-out users');

    changeListener(
        autofillManager.data.addresses, [], [], [], STUB_USER_ACCOUNT_INFO);
    assertTrue(
        isVisible(getIcon()),
        'Sync is disabled but the feature is on, the icon should be visible.');

    document.body.removeChild(section);
  });

  test('verifyNoAddressLocalIndicationForAccountNameEmail', async () => {
    const nameEmailAddress = createAddressEntry();
    nameEmailAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL;

    const autofillManager = new TestAutofillManager();
    autofillManager.data.addresses = [nameEmailAddress];
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    const addressList = section.$.addressList;
    const getIcon = () => addressList.children[0]!.querySelector<HTMLElement>(
        '#address-row-icon');
    const iconName = getIcon()!.getAttribute('icon');
    assertFalse(
        !!iconName && iconName.includes('cloud-off'),
        'Local indicator should not be shown on account name email profile');
    document.body.removeChild(section);
  });

  test('verifyAddressRowButtonTriggersDropdown', async function() {
    const address = createAddressEntry();
    const section = await createAutofillSection([address], {});
    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);
    const menuButton = row.querySelector<HTMLElement>('.address-menu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertTrue(!!section.shadowRoot!.querySelector('#menuEditAddress'));
  });

  test('verifyAccountHomeAddressEdit', async function() {
    const openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    const homeAddress = createAddressEntry();
    homeAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_HOME;
    const section = await createAutofillSection([homeAddress], {});

    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);
    const menuButton = row.querySelector<HTMLElement>('.address-menu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    const editButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuEditAddress');
    assertTrue(!!editButton);
    editButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('googleAccountHomeAddressUrl'));
  });

  test('verifyAccountWorkAddressEdit', async function() {
    const openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    const workAddress = createAddressEntry();
    workAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_WORK;
    const section = await createAutofillSection([workAddress], {});

    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);
    const menuButton = row.querySelector<HTMLElement>('.address-menu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    const editButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuEditAddress');
    assertTrue(!!editButton);
    editButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('googleAccountWorkAddressUrl'));
  });

  test('verifyAccountNameEmailAddressEdit', async function() {
    const openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    const nameEmailAddress = createAddressEntry();
    nameEmailAddress.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT_NAME_EMAIL;
    const section = await createAutofillSection([nameEmailAddress], {});

    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);
    const menuButton = row.querySelector<HTMLElement>('.address-menu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    const editButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuEditAddress');
    assertTrue(!!editButton);
    editButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        url, loadTimeData.getString('googleAccountNameEmailAddressEditUrl'));
  });

  test('verifyAddAddressDialog', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      const title = dialog.shadowRoot!.querySelector('[slot=title]')!;
      assertEquals(
          loadTimeData.getString('addAddressTitle'), title.textContent);
      // A country is preselected.
      const countrySelect = dialog.$.country;
      assertTrue(!!countrySelect);
      assertTrue(!!countrySelect.value);
    });
  });

  test('verifyEditAddressDialog', function() {
    return createAddressDialog(createAddressEntry()).then(function(dialog) {
      const title = dialog.shadowRoot!.querySelector('[slot=title]')!;
      assertEquals(
          loadTimeData.getString('editAddressTitle'), title.textContent);
      // Should be possible to save when editing because fields are
      // populated.
      assertFalse(dialog.$.saveButton.disabled);
    });
  });

  // The first editable element should be focused by default.
  test('verifyFirstFieldFocused', async function() {
    const dialog = await createAddressDialog(createEmptyAddressEntry());
    const currentFocus = dialog.shadowRoot!.activeElement;
    const editableElements =
        dialog.$.dialog.querySelectorAll('cr-input, select');
    assertEquals(editableElements[0], currentFocus);
  });

  test('verifyRemoveAddressDialogConfirmed', async function() {
    const autofillManager = new TestAutofillManager();
    const removeAddressDialog =
        await createRemoveAddressDialog(autofillManager);

    // Wait for the dialog to open.
    await whenAttributeIs(removeAddressDialog.$.dialog, 'open', '');

    removeAddressDialog.$.remove.click();

    // Wait for the dialog to close.
    await eventToPromise('close', removeAddressDialog);

    assertTrue(removeAddressDialog.wasConfirmed());
    assertEquals(
        1,
        metricsTracker.count('Autofill.ProfileDeleted.Settings.Account', true));
    assertEquals(
        0,
        metricsTracker.count(
            'Autofill.ProfileDeleted.Settings.Account', false));
    assertEquals(
        1,
        metricsTracker.count('Autofill.ProfileDeleted.Settings.Total', true));
    assertEquals(
        0,
        metricsTracker.count('Autofill.ProfileDeleted.Settings.Total', false));
    assertEquals(
        1, metricsTracker.count('Autofill.ProfileDeleted.Any.Account', true));
    assertEquals(
        0, metricsTracker.count('Autofill.ProfileDeleted.Any.Account', false));
    assertEquals(
        1, metricsTracker.count('Autofill.ProfileDeleted.Any.Total', true));
    assertEquals(
        0, metricsTracker.count('Autofill.ProfileDeleted.Any.Total', false));

    const expected = new AutofillManagerExpectations();
    expected.requestedAddresses = 1;
    expected.listeningAddresses = 1;
    expected.removeAddress = 1;
    autofillManager.assertExpectations(expected);
  });

  test('verifyRemoveAddressDialogCanceled', async function() {
    const autofillManager = new TestAutofillManager();
    const removeAddressDialog =
        await createRemoveAddressDialog(autofillManager);

    // Wait for the dialog to open.
    await whenAttributeIs(removeAddressDialog.$.dialog, 'open', '');

    removeAddressDialog.$.cancel.click();

    // Wait for the dialog to close.
    await eventToPromise('close', removeAddressDialog);
    assertFalse(removeAddressDialog.wasConfirmed());
    assertEquals(
        0,
        metricsTracker.count('Autofill.ProfileDeleted.Settings.Account', true));
    assertEquals(
        1,
        metricsTracker.count(
            'Autofill.ProfileDeleted.Settings.Account', false));
    assertEquals(
        0,
        metricsTracker.count('Autofill.ProfileDeleted.Settings.Total', true));
    assertEquals(
        1,
        metricsTracker.count('Autofill.ProfileDeleted.Settings.Total', false));
    assertEquals(
        0, metricsTracker.count('Autofill.ProfileDeleted.Any.Account', true));
    assertEquals(
        1, metricsTracker.count('Autofill.ProfileDeleted.Any.Account', false));
    assertEquals(
        0, metricsTracker.count('Autofill.ProfileDeleted.Any.Total', true));
    assertEquals(
        1, metricsTracker.count('Autofill.ProfileDeleted.Any.Total', false));

    const expected = new AutofillManagerExpectations();
    expected.requestedAddresses = 1;
    expected.listeningAddresses = 1;
    expected.removeAddress = 0;
    autofillManager.assertExpectations(expected);
  });

  test('verifyCountryIsSaved', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      const countrySelect = dialog.$.country;
      assertTrue(!!countrySelect);
      // The country should be pre-selected.
      assertEquals('US', countrySelect.value);
      countrySelect.value = 'GB';
      countrySelect.dispatchEvent(new CustomEvent('change'));
      flush();
      assertEquals('GB', countrySelect.value);
    });
  });

  test('verifyLanguageCodeIsSaved', function() {
    const address = createEmptyAddressEntry();
    // TODO(crbug.com/403312087): Don't nest promise callbacks (here and
    // everywhere else in this file). Use async/await instead. Check this
    // comment for more info: crrev.com/c/6348920/comment/25b26a8a_d69cf940/
    return createAddressDialog(address).then(function(dialog) {
      const countrySelect = dialog.$.country;
      assertTrue(!!countrySelect);
      // The first country is pre-selected.
      assertEquals('US', countrySelect.value);
      assertEquals('en', address.languageCode);
      countrySelect.value = 'IL';
      countryDetailManager.setGetAddressFormatRepsonse(ADDRESS_COMPONENTS_IL);
      countrySelect.dispatchEvent(new CustomEvent('change'));
      flush();
      return eventToPromise('on-update-address-wrapper', dialog)
          .then(function() {
            assertEquals('IL', countrySelect.value);
            assertEquals('iw', address.languageCode);
          });
    });
  });

  test('verifyPhoneAndEmailAreSaved', async () => {
    const address = createEmptyAddressEntry();
    const dialog = await createAddressDialog(address);
    const rows = dialog.$.dialog.querySelectorAll('.address-row');
    assertGT(rows.length, 0, 'dialog should contain address rows');

    const lastRow = rows[rows.length - 1]!;
    const phoneInput =
        lastRow.querySelector<CrInputElement>('cr-input:nth-of-type(1)');
    const emailInput =
        lastRow.querySelector<CrInputElement>('cr-input:nth-of-type(2)');

    assertTrue(!!phoneInput, 'phone element should be the first cr-input');
    assertTrue(!!emailInput, 'email element should be the second cr-input');

    assertEquals(undefined, phoneInput.value);
    assertFalse(
        !!getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER));

    assertEquals(undefined, emailInput.value);
    assertFalse(!!getAddressFieldValue(address, FieldType.EMAIL_ADDRESS));

    const phoneNumber = '(555) 555-5555';
    const emailAddress = 'no-reply@chromium.org';

    phoneInput.value = phoneNumber;
    emailInput.value = emailAddress;
    await Promise.all([phoneInput.updateComplete, emailInput.updateComplete]);
    await expectEvent(dialog, 'save-address', function() {
      dialog.$.saveButton.click();
    });
    assertEquals(phoneNumber, phoneInput.value);
    assertEquals(
        phoneNumber,
        getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER));

    assertEquals(emailAddress, emailInput.value);
    assertEquals(
        emailAddress, getAddressFieldValue(address, FieldType.EMAIL_ADDRESS));
  });

  test('verifyPhoneAndEmailAreRemoved', function() {
    const address = createEmptyAddressEntry();

    const phoneNumber = '(555) 555-5555';
    const emailAddress = 'no-reply@chromium.org';

    address.fields.push({
      type: FieldType.ADDRESS_HOME_COUNTRY,
      value: 'US',
    });  // Set to allow save to be active.
    address.fields.push({
      type: FieldType.PHONE_HOME_WHOLE_NUMBER,
      value: phoneNumber,
    });
    address.fields.push({type: FieldType.EMAIL_ADDRESS, value: emailAddress});

    return createAddressDialog(address).then(async function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertGT(rows.length, 0, 'dialog should contain address rows');

      const lastRow = rows[rows.length - 1]!;
      const phoneInput =
          lastRow.querySelector<CrInputElement>('cr-input:nth-of-type(1)');
      const emailInput =
          lastRow.querySelector<CrInputElement>('cr-input:nth-of-type(2)');

      assertTrue(!!phoneInput, 'phone element should be the first cr-input');
      assertTrue(!!emailInput, 'email element should be the second cr-input');

      assertEquals(
          phoneNumber, phoneInput.value,
          'The input should have the corresponding address field value.');
      assertEquals(
          emailAddress, emailInput.value,
          'The input should have the corresponding address field value.');

      phoneInput.value = '';
      emailInput.value = '';
      await flushTasks();

      return expectEvent(dialog, 'save-address', function() {
               dialog.$.saveButton.click();
             }).then(function() {
        assertFalse(
            !!getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER),
            'The phone field should be empty.');
        assertFalse(
            !!getAddressFieldValue(address, FieldType.EMAIL_ADDRESS),
            'The email field should be empty.');
      });
    });
  });

  // Test will set a value of 'foo' in each text field and verify that the
  // save button is enabled, then it will clear the field and verify that the
  // save button is disabled. Test passes after all elements have been tested.
  test('verifySaveIsNotClickableIfAllInputFieldsAreEmpty', async function() {
    const dialog = await createAddressDialog(createEmptyAddressEntry());
    const saveButton = dialog.$.saveButton;
    const testElements =
        dialog.$.dialog.querySelectorAll<CrTextareaElement|CrInputElement>(
            'cr-textarea, cr-input');

    // The country can be preselected. Clear it to ensure the form is empty.
    await expectEvent(dialog, 'on-update-can-save', function() {
      const countrySelect = dialog.$.country;
      assertTrue(!!countrySelect);
      countrySelect.value = '';
      countrySelect.dispatchEvent(new CustomEvent('change'));
    });
    assertEquals(
        6, testElements.length,
        'There should be 6 elements: The 4 fields from ' +
            '`ADDRESS_COMPONENTS_US` + phone + email that are added ' +
            'separately.');

    assertTrue(saveButton.disabled);
    for (const element of testElements) {
      await expectEvent(dialog, 'on-update-can-save', function() {
        element.value = 'foo';
      });
      assertFalse(saveButton.disabled);
      await expectEvent(dialog, 'on-update-can-save', function() {
        element.value = '';
      });
      assertTrue(saveButton.disabled);
    }
  });

  // Setting the country should allow the address to be saved.
  test('verifySaveIsNotClickableIfCountryNotSet', async function() {
    function simulateCountryChange(countryCode: string) {
      const countrySelect = dialog.$.country;
      assertTrue(!!countrySelect);
      countrySelect.value = countryCode;
      countrySelect.dispatchEvent(new CustomEvent('change'));
    }

    const dialog = await createAddressDialog(createEmptyAddressEntry());
    const countrySelect = dialog.$.country;
    assertTrue(!!countrySelect);
    // A country code is preselected.
    assertFalse(dialog.$.saveButton.disabled);
    assertEquals(countrySelect.value, 'US');

    await expectEvent(
        dialog, 'on-update-can-save', simulateCountryChange.bind(null, 'GB'));
    assertFalse(dialog.$.saveButton.disabled);

    await expectEvent(
        dialog, 'on-update-can-save', simulateCountryChange.bind(null, ''));
    assertTrue(dialog.$.saveButton.disabled);
  });

  // Test will timeout if save-address event is not fired.
  test('verifyDefaultCountryIsAppliedWhenSaving', function() {
    const address = createEmptyAddressEntry();
    address.fields.push({type: FieldType.NAME_FULL, value: 'Name'});
    return createAddressDialog(address).then(function(dialog) {
      return expectEvent(dialog, 'save-address', function() {
               // Verify |countryCode| is not set.
               assertEquals(
                   undefined,
                   getAddressFieldValue(
                       address, FieldType.ADDRESS_HOME_COUNTRY));
               dialog.$.saveButton.click();
             }).then(function(_event) {
        // 'US' is the default country for these tests.
        const countrySelect = dialog.$.country;
        assertTrue(!!countrySelect);
        assertEquals('US', countrySelect.value);
      });
    });
  });

  test(
      'verifyNoSaveAddressEventWhenEditDialogCancelButtonIsClicked',
      function(done) {
        createAddressDialog(createAddressEntry()).then(function(dialog) {
          eventToPromise('save-address', dialog).then(function() {
            // Fail the test because the save event should not be fired when
            // the cancel is clicked.
            assertTrue(true);
          });

          eventToPromise('cancel', dialog).then(function() {
            // Test is |done| in a timeout in order to ensure that
            // 'save-address' is NOT fired after this test.
            assertEquals(
                1,
                metricsTracker.count('Autofill.Settings.EditAddress', false));
            window.setTimeout(done, 100);
          });

          dialog.$.cancelButton.click();
        });
      });

  test('verifyNoCancelEventWhenEditDialogSaveButtonIsClicked', function(done) {
    createAddressDialog(createAddressEntry()).then(function(dialog) {
      eventToPromise('cancel', dialog).then(function() {
        // Fail the test because the cancel event should not be fired when
        // the save is clicked.
        assertTrue(false);
      });

      eventToPromise('save-address', dialog).then(function() {
        // Test is |done| in a timeout in order to ensure that
        // 'save-address' is NOT fired after this test.
        assertEquals(
            1, metricsTracker.count('Autofill.Settings.EditAddress', true));
        window.setTimeout(done, 100);
      });

      dialog.$.saveButton.click();
    });
  });

  test('verifySyncRecordTypeNoticeForNewAddress', async () => {
    const section = await createAutofillSection([], {}, {
      ...STUB_USER_ACCOUNT_INFO,
      email: 'stub-user@example.com',
      isSyncEnabledForAutofillProfiles: true,
      isEligibleForAddressAccountStorage: false,
    });

    const dialog = await openAddressDialog(section);

    assertTrue(
        !isVisible(dialog.$.accountRecordTypeNotice),
        'account notice should be invisible for non-account address');

    document.body.removeChild(section);
  });

  test('verifyAccountRecordTypeNoticeForNewAddress', async () => {
    const email = 'stub-user@example.com';
    const section = await createAutofillSection([], {}, {
      ...STUB_USER_ACCOUNT_INFO,
      email,
      isSyncEnabledForAutofillProfiles: true,
      isEligibleForAddressAccountStorage: true,
    });

    const dialog = await openAddressDialog(section);

    assertTrue(
        isVisible(dialog.$.accountRecordTypeNotice),
        'account notice should be visible as the user is eligible');

    assertEquals(
        dialog.$.accountRecordTypeNotice.innerText,
        section.i18n('newAccountAddressRecordTypeNotice', email));

    document.body.removeChild(section);
  });

  // TODO(crbug.com/40943238): Remove when toggle becomes available on the Sync
  // page for non-syncing users.
  test('verifyAutofillSyncToggleAvailability', async () => {
    const autofillManager = new TestAutofillManager();
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isAutofillSyncToggleAvailable: false,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    assertFalse(
        isVisible(section.$.autofillSyncToggle),
        'The toggle should not be visible because of ' +
            'accountInfo.isAutofillSyncToggleAvailable == false');
    const changeListener =
        autofillManager.lastCallback.setPersonalDataManagerListener;
    assertTrue(
        !!changeListener,
        'PersonalDataChangedListener should be set in the section element');

    // Imitate native code `PersonalDataChangedListener` triggering.
    changeListener([], [], [], [], {
      ...STUB_USER_ACCOUNT_INFO,
      isAutofillSyncToggleAvailable: true,
      isAutofillSyncToggleEnabled: false,
    });

    await flushTasks();
    assertTrue(
        isVisible(section.$.autofillSyncToggle),
        'The toggle should be visible because of ' +
            'accountInfo.isAutofillSyncToggleAvailable == true');
    assertFalse(
        section.$.autofillSyncToggle.checked,
        'The toggle should not be checked because of ' +
            'accountInfo.isAutofillSyncToggleEnabled == false');

    // Imitate native code `PersonalDataChangedListener` triggering.
    changeListener([], [], [], [], {
      ...STUB_USER_ACCOUNT_INFO,
      isAutofillSyncToggleAvailable: true,
      isAutofillSyncToggleEnabled: true,
    });

    await flushTasks();
    assertTrue(
        isVisible(section.$.autofillSyncToggle),
        'The toggle should be visible because of ' +
            'accountInfo.isAutofillSyncToggleAvailable == true');
    assertTrue(
        section.$.autofillSyncToggle.checked,
        'The toggle should be checked because of ' +
            'accountInfo.isAutofillSyncToggleEnabled == true');
  });

  // TODO(crbug.com/40943238): Remove as part of the cleanup work for the ticket.
  test('verifyAutofillSyncToggleChanges', async () => {
    const autofillManager = new TestAutofillManager();
    autofillManager.data.accountInfo = {
      ...STUB_USER_ACCOUNT_INFO,
      isAutofillSyncToggleAvailable: true,
      isAutofillSyncToggleEnabled: false,
    };
    AutofillManagerImpl.setInstance(autofillManager);

    const section = document.createElement('settings-autofill-section');
    document.body.appendChild(section);
    await autofillManager.whenCalled('getAddressList');
    await flushTasks();

    const changeListener =
        autofillManager.lastCallback.setPersonalDataManagerListener;
    assertTrue(
        !!changeListener,
        'PersonalDataChangedListener should be set in the section element');

    assertFalse(
        section.$.autofillSyncToggle.checked,
        'The toggle should not be checked because of initial ' +
            'accountInfo.isAutofillSyncToggleEnabled == false');

    section.$.autofillSyncToggle.click();
    await section.$.autofillSyncToggle.updateComplete;

    assertTrue(
        section.$.autofillSyncToggle.checked,
        'The toggle should be checked after the click.');
    assertEquals(
        autofillManager.getCallCount('setAutofillSyncToggleEnabled'), 1);

    section.$.autofillSyncToggle.click();
    await section.$.autofillSyncToggle.updateComplete;

    assertFalse(
        section.$.autofillSyncToggle.checked,
        'The toggle should not be checked after another click.');
    assertEquals(
        autofillManager.getCallCount('setAutofillSyncToggleEnabled'), 2);

    // Imitate native code `PersonalDataChangedListener` triggering. Notice
    // that it was unchecked after the second click, but the listener was
    // given `true`, the following assert checks it an covers the case when
    // the toggle was not updated in the native code for some reason.
    changeListener([], [], [], [], {
      ...STUB_USER_ACCOUNT_INFO,
      isAutofillSyncToggleAvailable: true,
      isAutofillSyncToggleEnabled: true,
    });
    await flushTasks();

    assertTrue(
        section.$.autofillSyncToggle.checked,
        'The toggle should be checked because of ' +
            'accountInfo.isAutofillSyncToggleEnabled == true');
  });
});

suite('AutofillSectionAddressLocaleTests', function() {
  let countryDetailManager: TestCountryDetailManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    countryDetailManager = new TestCountryDetailManagerProxy();
    CountryDetailManagerProxyImpl.setInstance(countryDetailManager);

    countryDetailManager.setGetCountryListRepsonse([
      {name: 'United States', countryCode: 'US'},  // Default country.
      {name: 'Israel', countryCode: 'IL'},
      {name: 'United Kingdom', countryCode: 'GB'},
    ]);
    countryDetailManager.setGetAddressFormatRepsonse(ADDRESS_COMPONENTS_US);
  });

  // US address has 3 fields on the same line.
  test('verifyEditingUSAddress', function() {
    const address = createEmptyAddressEntry();

    address.fields = [
      {type: FieldType.ADDRESS_HOME_COUNTRY, value: 'US'},
      {type: FieldType.NAME_FULL, value: 'Name'},
      {type: FieldType.ADDRESS_HOME_CITY, value: 'City'},
      {type: FieldType.ADDRESS_HOME_STATE, value: 'State'},
      {type: FieldType.ADDRESS_HOME_ZIP, value: 'ZIP code'},
      {type: FieldType.PHONE_HOME_WHOLE_NUMBER, value: 'Phone'},
      {type: FieldType.EMAIL_ADDRESS, value: 'Email'},
    ];

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(
          4, rows.length,
          'There should be 4 rows: Country, Name, City + State + Zip, ' +
              'Phone + Email');

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'United States',
          countrySelect.selectedOptions[0]!.textContent.trim());
      index++;
      // Name
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.NAME_FULL), cols[0]!.value);
      index++;
      // City, State, ZIP code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(3, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_CITY),
          cols[0]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_STATE),
          cols[1]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_ZIP),
          cols[2]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER),
          cols[0]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.EMAIL_ADDRESS),
          cols[1]!.value);
    });
  });

  // GB address has 1 field per line for all lines that change.
  test('verifyEditingGBAddress', function() {
    const address = createEmptyAddressEntry();

    address.fields = [
      {type: FieldType.ADDRESS_HOME_COUNTRY, value: 'GB'},
      {type: FieldType.NAME_FULL, value: 'Name'},
      {type: FieldType.ADDRESS_HOME_CITY, value: 'Post town'},
      {type: FieldType.ADDRESS_HOME_ZIP, value: 'Postal code'},
      {type: FieldType.ADDRESS_HOME_STATE, value: 'County'},
      {type: FieldType.PHONE_HOME_WHOLE_NUMBER, value: 'Phone'},
      {type: FieldType.EMAIL_ADDRESS, value: 'Email'},
    ];

    countryDetailManager.setGetAddressFormatRepsonse(ADDRESS_COMPONENTS_GB);
    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(
          6, rows.length,
          'There should be 6 rows: Country, Name, City, Zip, State, ' +
              'Phone + Email');

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'United Kingdom',
          countrySelect.selectedOptions[0]!.textContent.trim());
      index++;
      // Name
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.NAME_FULL), cols[0]!.value);
      index++;
      // Post Town
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_CITY),
          cols[0]!.value);
      index++;
      // Postal code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_ZIP),
          cols[0]!.value);
      index++;
      // County
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_STATE),
          cols[0]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER),
          cols[0]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.EMAIL_ADDRESS),
          cols[1]!.value);
    });
  });

  // IL address has 2 fields on the same line and is an RTL locale.
  // RTL locale shouldn't affect this test.
  test('verifyEditingILAddress', function() {
    const address = createEmptyAddressEntry();
    address.fields = [
      {type: FieldType.ADDRESS_HOME_COUNTRY, value: 'IL'},
      {type: FieldType.NAME_FULL, value: 'Name'},
      {type: FieldType.ADDRESS_HOME_CITY, value: 'City'},
      {type: FieldType.ADDRESS_HOME_ZIP, value: 'Postal code'},
      {type: FieldType.PHONE_HOME_WHOLE_NUMBER, value: 'Phone'},
      {type: FieldType.EMAIL_ADDRESS, value: 'Email'},
    ];

    countryDetailManager.setGetAddressFormatRepsonse(ADDRESS_COMPONENTS_IL);
    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(
          4, rows.length,
          'There should be 4 rows: Country, Name, City + Zip, Phone + Email');

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'Israel', countrySelect.selectedOptions[0]!.textContent.trim());
      index++;
      // Name
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.NAME_FULL)!, cols[0]!.value);
      index++;
      // City, Postal code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_CITY),
          cols[0]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_ZIP),
          cols[1]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER),
          cols[0]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.EMAIL_ADDRESS),
          cols[1]!.value);
    });
  });

  // Testing address edit dialog in an RTL layout by setting the document
  // direction to 'rtl'. The phone number input should have direction=ltr and
  // text-align=end.
  test('verifyEditingILAddressWithRtlLayout', function() {
    document.documentElement.dir = 'rtl';
    const address = createEmptyAddressEntry();
    address.fields = [
      {type: FieldType.ADDRESS_HOME_COUNTRY, value: 'IL'},
      {type: FieldType.NAME_FULL, value: 'Name'},
      {type: FieldType.ADDRESS_HOME_CITY, value: 'City'},
      {type: FieldType.ADDRESS_HOME_ZIP, value: 'Postal code'},
      {type: FieldType.PHONE_HOME_WHOLE_NUMBER, value: 'Phone'},
      {type: FieldType.EMAIL_ADDRESS, value: 'Email'},
    ];

    countryDetailManager.setGetAddressFormatRepsonse(ADDRESS_COMPONENTS_IL);
    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      // There should be 4 rows: Country, Name, City + Zip, Phone + Email
      assertEquals(4, rows.length);

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'Israel', countrySelect.selectedOptions[0]!.textContent.trim());
      index++;
      // Name
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.NAME_FULL)!, cols[0]!.value);
      index++;
      // City, Postal code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_CITY),
          cols[0]!.value);
      assertEquals(
          getAddressFieldValue(address, FieldType.ADDRESS_HOME_ZIP),
          cols[1]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(
          getAddressFieldValue(address, FieldType.PHONE_HOME_WHOLE_NUMBER),
          cols[0]!.value);
      assertTrue(cols[0]!.classList.contains('phone-number-input'));
      const phoneInput = (cols[0]! as CrInputElement).inputElement;
      assertEquals(
          'ltr',
          (phoneInput.computedStyleMap().get('direction') as CSSUnitValue)
              .value);
      assertEquals(
          'end',
          (phoneInput.computedStyleMap().get('text-align') as CSSUnitValue)
              .value);
      assertEquals(
          getAddressFieldValue(address, FieldType.EMAIL_ADDRESS),
          cols[1]!.value);
    });
  });

  // US has an extra field 'State'. Validate that this field is
  // persisted when switching to IL then back to US.
  test('verifyAddressPersistanceWhenSwitchingCountries', function() {
    const address = createEmptyAddressEntry();
    address.fields.push({type: FieldType.ADDRESS_HOME_COUNTRY, value: 'US'});

    return createAddressDialog(address).then(function(dialog) {
      const city = 'Los Angeles';
      const state = 'CA';
      const zip = '90291';
      const countrySelect = dialog.$.country;
      assertTrue(!!countrySelect);

      return expectEvent(
                 dialog, 'on-update-address-wrapper',
                 function() {
                   // US:
                   const rows =
                       dialog.$.dialog.querySelectorAll('.address-row');
                   assertEquals(
                       4, rows.length,
                       'There should be 4 rows: Country, Name, City + State ' +
                           '+ Zip, Phone + Email');

                   // City, State, ZIP code
                   const row = rows[2]!;
                   const cols =
                       row.querySelectorAll<CrTextareaElement|CrInputElement>(
                           '.address-column');
                   assertEquals(3, cols.length);
                   cols[0]!.value = city;
                   cols[1]!.value = state;
                   cols[2]!.value = zip;

                   countryDetailManager.setGetAddressFormatRepsonse(
                       ADDRESS_COMPONENTS_IL);
                   countrySelect.value = 'IL';
                   countrySelect.dispatchEvent(new CustomEvent('change'));
                 })
          .then(function() {
            return expectEvent(dialog, 'on-update-address-wrapper', function() {
              // IL:
              const rows = dialog.$.dialog.querySelectorAll('.address-row');
              assertEquals(
                  4, rows.length,
                  'There should be 4 rows: Country, Name, City + Zip, ' +
                      'Phone + Email');

              // City, Postal code
              const row = rows[2]!;
              const cols =
                  row.querySelectorAll<CrTextareaElement|CrInputElement>(
                      '.address-column');
              assertEquals(2, cols.length);
              assertEquals(city, cols[0]!.value);
              assertEquals(zip, cols[1]!.value);

              countryDetailManager.setGetAddressFormatRepsonse(
                  ADDRESS_COMPONENTS_US);
              countrySelect.value = 'US';
              countrySelect.dispatchEvent(new CustomEvent('change'));
            });
          })
          .then(function() {
            // US:
            const rows = dialog.$.dialog.querySelectorAll('.address-row');
            assertEquals(
                4, rows.length,
                'There should be 4 rows: Country, Name, City + State + Zip, ' +
                    'Phone + Email');

            // City, State, ZIP code
            const row = rows[2]!;
            const cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
                '.address-column');
            assertEquals(3, cols.length);
            assertEquals(city, cols[0]!.value);
            assertEquals(state, cols[1]!.value);
            assertEquals(zip, cols[2]!.value);
          });
    });
  });
});

suite('PlusAddressesTest', function() {
  const fakeUrl = 'https://foo.bar';
  let metrics: MetricsTracker;
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    metrics = fakeMetricsPrivate();
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    loadTimeData.overrideValues({
      // Required to show the plus address management entry.
      plusAddressEnabled: true,
      plusAddressManagementUrl: fakeUrl,
    });
  });

  test('verifyPlusAddressManagementEntryExistence', async function() {
    const autofillSection = await createAutofillSection([], {});

    const plusAddressButton =
        autofillSection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#plusAddressSettingsButton');
    assertTrue(!!plusAddressButton);

    autofillSection.remove();
  });

  test(
      'verifyPlusAddressManagementEntryExistenceWhenNotEnabled',
      async function() {
        loadTimeData.overrideValues({
          plusAddressEnabled: false,
        });
        const autofillSection = await createAutofillSection([], {});

        const plusAddressButton =
            autofillSection.shadowRoot!.querySelector<CrLinkRowElement>(
                '#plusAddressSettingsButton');
        assertFalse(!!plusAddressButton);

        autofillSection.remove();
      });

  test(
      'verifyClickingPlusAddressManagementEntryOpensWebsite', async function() {
        const autofillSection = await createAutofillSection([], {});

        const plusAddressButton =
            autofillSection.shadowRoot!.querySelector<CrLinkRowElement>(
                '#plusAddressSettingsButton');
        assertTrue(!!plusAddressButton);
        // Validate that, when present, the button results in opening the URL
        // passed in via the `loadTimeData` override.
        plusAddressButton.click();
        const url = await openWindowProxy.whenCalled('openUrl');
        assertEquals(url, fakeUrl);
        assertEquals(
            1, metrics.count('Settings.ManageOptionOnSettingsSelected'));
        autofillSection.remove();
      });
});
