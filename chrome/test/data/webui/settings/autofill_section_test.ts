// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManagerImpl, CountryDetailManagerImpl, CrInputElement, CrTextareaElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, whenAttributeIs, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {AutofillManagerExpectations, createAddressEntry, createEmptyAddressEntry, STUB_USER_ACCOUNT_INFO, TestAutofillManager} from './autofill_fake_data.js';
import {createAutofillSection, initiateRemoving, initiateEditing, CountryDetailManagerTestImpl, createAddressDialog, createRemoveAddressDialog, expectEvent, openAddressDialog, deleteAddress} from './autofill_section_test_utils.js';
// clang-format on

suite('AutofillSectionUiTest', function() {
  test('testAutofillExtensionIndicator', function() {
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

  test('verifyAddressDeleteSourceNotice', async () => {
    const address = createAddressEntry();
    const accountAddress = createAddressEntry();
    accountAddress.metadata!.source =
        chrome.autofillPrivate.AddressSource.ACCOUNT;

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
      assertTrue(
          !isVisible(dialog.$.accountAddressDescription),
          'account notice should be invisible for non-account address');
      assertTrue(
          !isVisible(dialog.$.localAddressDescription),
          'sync is enabled, an appropriate message should be visible');
      assertTrue(
          isVisible(dialog.$.syncAddressDescription),
          'sync is enabled, an appropriate message should be visible');
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
    changeListener(autofillManager.data.addresses, [], [], {
      ...STUB_USER_ACCOUNT_INFO,
    });

    {
      const dialog = await initiateRemoving(section, 0);
      assertTrue(
          !isVisible(dialog.$.accountAddressDescription),
          'account notice should be invisible for non-account address');
      assertTrue(
          isVisible(dialog.$.localAddressDescription),
          'sync is disabled, an appropriate message should be visible');
      assertTrue(
          !isVisible(dialog.$.syncAddressDescription),
          'sync is disabled, an appropriate message should be visible');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    // Imitate disabling sync.
    changeListener(autofillManager.data.addresses, [], [], undefined);

    {
      const dialog = await initiateRemoving(section, 0);
      assertTrue(
          !isVisible(dialog.$.accountAddressDescription),
          'account notice should be invisible for non-account address');
      assertTrue(
          isVisible(dialog.$.localAddressDescription),
          'sync is disabled, an appropriate message should be visible');
      assertTrue(
          !isVisible(dialog.$.syncAddressDescription),
          'sync is disabled, an appropriate message should be visible');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    changeListener(autofillManager.data.addresses, [], [], {
      ...STUB_USER_ACCOUNT_INFO,
      isSyncEnabledForAutofillProfiles: true,
    });

    {
      const dialog = await initiateRemoving(section, 1);
      assertTrue(
          isVisible(dialog.$.accountAddressDescription),
          'account notice should be visible for non-account address');
      assertTrue(
          !isVisible(dialog.$.localAddressDescription),
          'non-account messages should not be visible');
      assertTrue(
          !isVisible(dialog.$.syncAddressDescription),
          'non-account messages should not be visible');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });

  test('verifyAddressEditSourceNotice', async () => {
    const email = 'stub-user@example.com';
    const address = createAddressEntry();
    const accouontAddress = createAddressEntry();
    accouontAddress.metadata!.source =
        chrome.autofillPrivate.AddressSource.ACCOUNT;
    const section =
        await createAutofillSection([address, accouontAddress], {}, {
          ...STUB_USER_ACCOUNT_INFO,
          email,
        });

    {
      const dialog = await initiateEditing(section, 0);
      assertFalse(
          isVisible(dialog.$.accountSourceNotice),
          'account notice should be invisible for non-account address');
      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    await flushTasks();

    {
      const dialog = await initiateEditing(section, 1);
      assertTrue(
          isVisible(dialog.$.accountSourceNotice),
          'account notice should be visible for account address');

      assertEquals(
          dialog.$.accountSourceNotice.innerText,
          section.i18n('editAccountAddressSourceNotice', email));

      dialog.$.dialog.close();
      // Make sure closing clean-ups are finished.
      await eventToPromise('close', dialog.$.dialog);
    }

    document.body.removeChild(section);
  });
});

suite('AutofillSectionFocusTest', function() {
  test('verifyFocusLocationAfterRemoving', async () => {
    const section = await createAutofillSection(
        [
          createAddressEntry(),
          createAddressEntry(),
          createAddressEntry(),
        ],
        {profile_enabled: {value: true}});
    const manager = AutofillManagerImpl.getInstance() as TestAutofillManager;

    await deleteAddress(section, manager, 1);
    const addressesAfterRemovingInTheMiddle =
        section.$.addressList.querySelectorAll('.list-item');
    assertTrue(
        addressesAfterRemovingInTheMiddle[1]!.matches(':focus-within'),
        'The focus should remain on the same index on the list (but next ' +
            'to the removed address).');

    await deleteAddress(section, manager, 1);
    const addressesAfterRemovingLastInTheList =
        section.$.addressList.querySelectorAll('.list-item');
    assertTrue(
        addressesAfterRemovingLastInTheList[0]!.matches(':focus-within'),
        'After removing the last address on the list the focus should go ' +
            'to the preivous address.');

    await deleteAddress(section, manager, 0);
    assertTrue(
        section.$.addAddress.matches(':focus-within'),
        'If there are no addresses remaining after removal the focus should ' +
            'go to the Add button.');

    document.body.removeChild(section);
  });
});

suite('AutofillSectionAddressTests', function() {
  suiteSetup(function() {
    CountryDetailManagerImpl.setInstance(new CountryDetailManagerTestImpl());
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    const addressPieces = row!.querySelector('#addressSummary')!.children;
    for (const addressPiece of addressPieces) {
      actualSummary += addressPiece.textContent!.trim();
    }

    assertEquals(addressSummary, actualSummary);
  });

  test('verifyAddressLocalIndication', async () => {
    loadTimeData.overrideValues({
      autofillAccountProfileStorage: false,
      syncEnableContactInfoDataType: false,
      syncEnableContactInfoDataTypeInTransportMode: false,
    });

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

    assertFalse(
        isVisible(addressList.children[0]!.querySelector('[icon*=cloud-off]')),
        'Sync for addresses is enabled, the local indicator should be off.');

    const changeListener =
        autofillManager.lastCallback.setPersonalDataManagerListener!;

    changeListener(autofillManager.data.addresses, [], [], STUB_USER_ACCOUNT_INFO);
    assertFalse(
        isVisible(addressList.children[0]!.querySelector('[icon*=cloud-off]')),
        'Sync is disabled but the feature is off, the icon should be hidden.');

    changeListener(autofillManager.data.addresses, [], [], undefined);
    assertFalse(
        isVisible(section.$.addressList.children[0]!.querySelector(
            '[icon*=cloud-off]')),
        'The local indicator should not be shown to logged-out users');


    loadTimeData.overrideValues({
      autofillAccountProfileStorage: true,
      syncEnableContactInfoDataType: true,
      syncEnableContactInfoDataTypeInTransportMode: true,
    });
    changeListener(
        autofillManager.data.addresses, [], [], STUB_USER_ACCOUNT_INFO);
    assertTrue(
        isVisible(addressList.children[0]!.querySelector('[icon*=cloud-off]')),
        'Sync is disabled but the feature is on, the icon should be visible.');


    document.body.removeChild(section);
  });

  test('verifyAddressRowButtonTriggersDropdown', async function() {
    const address = createAddressEntry();
    const section = await createAutofillSection([address], {});
    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);
    const menuButton = row!.querySelector<HTMLElement>('.address-menu');
    assertTrue(!!menuButton);
    menuButton!.click();
    flush();

    assertTrue(!!section.shadowRoot!.querySelector('#menuEditAddress'));
    assertTrue(!!section.$.menuRemoveAddress);
  });

  test('verifyAddAddressDialog', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      const title = dialog.shadowRoot!.querySelector('[slot=title]')!;
      assertEquals(
          loadTimeData.getString('addAddressTitle'), title.textContent);
      // A country is preselected.
      assertTrue(!!address.countryCode);
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
    const expected = new AutofillManagerExpectations();
    expected.requestedAddresses = 1;
    expected.listeningAddresses = 1;
    expected.removeAddress = 0;
    autofillManager.assertExpectations(expected);
  });

  test('verifyCountryIsSaved', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      const countrySelect = dialog.shadowRoot!.querySelector('select')!;
      // The country should be pre-selected.
      assertEquals('US', countrySelect.value);
      assertEquals('US', address.countryCode);
      countrySelect.value = 'GB';
      countrySelect.dispatchEvent(new CustomEvent('change'));
      flush();
      assertEquals('GB', countrySelect.value);
      assertEquals('GB', address.countryCode);
    });
  });

  test('verifyLanguageCodeIsSaved', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      const countrySelect = dialog.shadowRoot!.querySelector('select')!;
      // The first country is pre-selected.
      assertEquals('US', address.countryCode);
      assertEquals('en', address.languageCode);
      countrySelect.value = 'IL';
      countrySelect.dispatchEvent(new CustomEvent('change'));
      flush();
      return eventToPromise('on-update-address-wrapper', dialog)
          .then(function() {
            assertEquals('IL', address.countryCode);
            assertEquals('iw', address.languageCode);
          });
    });
  });

  test('verifyPhoneAndEmailAreSaved', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
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
      assertFalse(!!address.phoneNumber);

      assertEquals(undefined, emailInput.value);
      assertFalse(!!address.emailAddress);

      const phoneNumber = '(555) 555-5555';
      const emailAddress = 'no-reply@chromium.org';

      phoneInput.value = phoneNumber;
      emailInput.value = emailAddress;

      return expectEvent(dialog, 'save-address', function() {
               dialog.$.saveButton.click();
             }).then(function() {
        assertEquals(phoneNumber, phoneInput.value);
        assertEquals(phoneNumber, address.phoneNumber);

        assertEquals(emailAddress, emailInput.value);
        assertEquals(emailAddress, address.emailAddress);
      });
    });
  });

  test('verifyHonorificIsSaved', async function() {
    loadTimeData.overrideValues({showHonorific: true});
    const address = createEmptyAddressEntry();
    const dialog = await createAddressDialog(address);
    const honorificElement =
        dialog.$.dialog.querySelectorAll<CrTextareaElement|CrInputElement>(
            'cr-textarea, cr-input')[0]!;
    assertEquals(undefined, honorificElement.value);
    assertFalse(!!address.honorific);

    const honorific = 'Lord';
    honorificElement.value = honorific;

    await expectEvent(
        dialog, 'save-address', () => dialog.$.saveButton.click());
    assertEquals(honorific, honorificElement.value);
    assertEquals(honorific, address.honorific);
  });

  test('verifyPhoneAndEmailAreRemoved', function() {
    const address = createEmptyAddressEntry();

    const phoneNumber = '(555) 555-5555';
    const emailAddress = 'no-reply@chromium.org';

    address.countryCode = 'US';  // Set to allow save to be active.
    address.phoneNumber = phoneNumber;
    address.emailAddress = emailAddress;

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertGT(rows.length, 0, 'dialog should contain address rows');

      const lastRow = rows[rows.length - 1]!;
      const phoneInput =
          lastRow.querySelector<CrInputElement>('cr-input:nth-of-type(1)');
      const emailInput =
          lastRow.querySelector<CrInputElement>('cr-input:nth-of-type(2)');

      assertTrue(!!phoneInput, 'phone element should be the first cr-input');
      assertTrue(!!emailInput, 'email element should be the second cr-input');

      assertEquals(phoneNumber, phoneInput.value);
      assertEquals(emailAddress, emailInput.value);

      phoneInput.value = '';
      emailInput.value = '';

      return expectEvent(dialog, 'save-address', function() {
               dialog.$.saveButton.click();
             }).then(function() {
        assertFalse(!!address.phoneNumber);
        assertFalse(!!address.emailAddress);
      });
    });
  });

  // Test will set a value of 'foo' in each text field and verify that the
  // save button is enabled, then it will clear the field and verify that the
  // save button is disabled. Test passes after all elements have been tested.
  test('verifySaveIsNotClickableIfAllInputFieldsAreEmpty', async function() {
    loadTimeData.overrideValues({showHonorific: true});
    const dialog = await createAddressDialog(createEmptyAddressEntry());
    const saveButton = dialog.$.saveButton;
    const testElements =
        dialog.$.dialog.querySelectorAll<CrTextareaElement|CrInputElement>(
            'cr-textarea, cr-input');

    // The country can be preselected. Clear it to ensure the form is empty.
    await expectEvent(dialog, 'on-update-can-save', function() {
      const countrySelect = dialog.shadowRoot!.querySelector('select')!;
      countrySelect.value = '';
      countrySelect.dispatchEvent(new CustomEvent('change'));
    });

    // Default country is 'US' expecting: Honorific, Name, Organization,
    // Street address, City, State, ZIP code, Phone, and Email.
    // Unless Company name or honorific is disabled.
    assertEquals(9, testElements.length);

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
      const countrySelect = dialog.shadowRoot!.querySelector('select')!;
      countrySelect.value = countryCode;
      countrySelect.dispatchEvent(new CustomEvent('change'));
    }

    const dialog = await createAddressDialog(createEmptyAddressEntry());
    // A country code is preselected.
    assertFalse(dialog.$.saveButton.disabled);
    assertEquals(dialog.address.countryCode, 'US');

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
    address.fullName = 'Name';
    return createAddressDialog(address).then(function(dialog) {
      return expectEvent(dialog, 'save-address', function() {
               // Verify |countryCode| is not set.
               assertEquals(undefined, address.countryCode);
               dialog.$.saveButton.click();
             }).then(function(event) {
        // 'US' is the default country for these tests.
        assertEquals('US', event.detail.countryCode);
      });
    });
  });

  test('verifyCancelDoesNotSaveAddress', function(done) {
    createAddressDialog(createAddressEntry()).then(function(dialog) {
      eventToPromise('save-address', dialog).then(function() {
        // Fail the test because the save event should not be called when
        // cancel is clicked.
        assertTrue(false);
      });

      eventToPromise('close', dialog).then(function() {
        // Test is |done| in a timeout in order to ensure that
        // 'save-address' is NOT fired after this test.
        window.setTimeout(done, 100);
      });

      dialog.$.cancelButton.click();
    });
  });

  test('verifySyncSourceNoticeForNewAddress', async () => {
    const section = await createAutofillSection([], {}, {
      email: 'stub-user@example.com',
      isSyncEnabledForAutofillProfiles: true,
      isEligibleForAddressAccountStorage: false,
    });

    const dialog = await openAddressDialog(section);

    assertTrue(
        !isVisible(dialog.$.accountSourceNotice),
        'account notice should be invisible for non-account address');

    document.body.removeChild(section);
  });

  test('verifyAccountSourceNoticeForNewAddress', async () => {
    const email = 'stub-user@example.com';
    const section = await createAutofillSection([], {}, {
      email,
      isSyncEnabledForAutofillProfiles: true,
      isEligibleForAddressAccountStorage: true,
    });

    const dialog = await openAddressDialog(section);

    assertTrue(
        isVisible(dialog.$.accountSourceNotice),
        'account notice should be visible as the user is eligible');

    assertEquals(
        dialog.$.accountSourceNotice.innerText,
        section.i18n('newAccountAddressSourceNotice', email));

    document.body.removeChild(section);
  });
});

suite('AutofillSectionAddressLocaleTests', function() {
  suiteSetup(function() {
    CountryDetailManagerImpl.setInstance(new CountryDetailManagerTestImpl());
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  // US address has 3 fields on the same line.
  test('verifyEditingUSAddress', function() {
    loadTimeData.overrideValues({showHonorific: true});
    const address = createEmptyAddressEntry();

    address.honorific = 'Honorific';
    address.fullName = 'Name';
    address.companyName = 'Organization';
    address.addressLines = 'Street address';
    address.addressLevel1 = 'State';
    address.addressLevel2 = 'City';
    address.postalCode = 'ZIP code';
    address.countryCode = 'US';
    address.phoneNumber = 'Phone';
    address.emailAddress = 'Email';

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(7, rows.length);

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'United States',
          countrySelect!.selectedOptions[0]!.textContent!.trim());
      index++;
      // Honorific
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.honorific, cols[0]!.value);
      index++;
      // Name
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.fullName, cols[0]!.value);
      index++;
      // Organization
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.companyName, cols[0]!.value);
      index++;
      // Street address
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLines, cols[0]!.value);
      index++;
      // City, State, ZIP code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(3, cols.length);
      assertEquals(address.addressLevel2, cols[0]!.value);
      assertEquals(address.addressLevel1, cols[1]!.value);
      assertEquals(address.postalCode, cols[2]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.phoneNumber, cols[0]!.value);
      assertEquals(address.emailAddress, cols[1]!.value);
    });
  });

  // GB address has 1 field per line for all lines that change.
  test('verifyEditingGBAddress', function() {
    loadTimeData.overrideValues({showHonorific: true});
    const address = createEmptyAddressEntry();

    address.honorific = 'Lord';
    address.fullName = 'Name';
    address.companyName = 'Organization';
    address.addressLines = 'Street address';
    address.addressLevel1 = 'County';
    address.addressLevel2 = 'Post town';
    address.postalCode = 'Postal code';
    address.countryCode = 'GB';
    address.phoneNumber = 'Phone';
    address.emailAddress = 'Email';

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(9, rows.length);

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'United Kingdom',
          countrySelect!.selectedOptions[0]!.textContent!.trim());
      index++;
      // Honorific
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.honorific, cols[0]!.value);
      index++;
      // Name
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.fullName, cols[0]!.value);
      index++;
      // Organization
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.companyName, cols[0]!.value);
      index++;
      // Street address
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLines, cols[0]!.value);
      index++;
      // Post Town
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLevel2, cols[0]!.value);
      index++;
      // Postal code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.postalCode, cols[0]!.value);
      index++;
      // County
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLevel1, cols[0]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.phoneNumber, cols[0]!.value);
      assertEquals(address.emailAddress, cols[1]!.value);
    });
  });

  // IL address has 2 fields on the same line and is an RTL locale.
  // RTL locale shouldn't affect this test.
  test('verifyEditingILAddress', function() {
    loadTimeData.overrideValues({showHonorific: true});
    const address = createEmptyAddressEntry();

    address.honorific = 'Honorific';
    address.fullName = 'Name';
    address.companyName = 'Organization';
    address.addressLines = 'Street address';
    address.addressLevel2 = 'City';
    address.postalCode = 'Postal code';
    address.countryCode = 'IL';
    address.phoneNumber = 'Phone';
    address.emailAddress = 'Email';

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(7, rows.length);

      let index = 0;
      // Country
      let row = rows[index]!;
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'Israel', countrySelect!.selectedOptions[0]!.textContent!.trim());
      index++;
      // Honorific
      row = rows[index]!;
      let cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.honorific, cols[0]!.value);
      index++;
      // Name
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.fullName!, cols[0]!.value);
      index++;
      // Organization
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.companyName, cols[0]!.value);
      index++;
      // Street address
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLines, cols[0]!.value);
      index++;
      // City, Postal code
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.addressLevel2, cols[0]!.value);
      assertEquals(address.postalCode, cols[1]!.value);
      index++;
      // Phone, Email
      row = rows[index]!;
      cols = row.querySelectorAll<CrTextareaElement|CrInputElement>(
          '.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.phoneNumber, cols[0]!.value);
      assertEquals(address.emailAddress, cols[1]!.value);
    });
  });

  // US has an extra field 'State'. Validate that this field is
  // persisted when switching to IL then back to US.
  test('verifyAddressPersistanceWhenSwitchingCountries', function() {
    loadTimeData.overrideValues({showHonorific: true});
    const address = createEmptyAddressEntry();
    const experimental_fields_count = 2;
    address.countryCode = 'US';

    return createAddressDialog(address).then(function(dialog) {
      const city = 'Los Angeles';
      const state = 'CA';
      const zip = '90291';
      const countrySelect = dialog.shadowRoot!.querySelector('select')!;

      return expectEvent(
                 dialog, 'on-update-address-wrapper',
                 function() {
                   // US:
                   const rows =
                       dialog.$.dialog.querySelectorAll('.address-row');
                   assertEquals(5 + experimental_fields_count, rows.length);

                   // City, State, ZIP code
                   const row = rows[3 + experimental_fields_count]!;
                   const cols =
                       row.querySelectorAll<CrTextareaElement|CrInputElement>(
                           '.address-column');
                   assertEquals(3, cols.length);
                   cols[0]!.value = city;
                   cols[1]!.value = state;
                   cols[2]!.value = zip;

                   countrySelect.value = 'IL';
                   countrySelect.dispatchEvent(new CustomEvent('change'));
                 })
          .then(function() {
            return expectEvent(dialog, 'on-update-address-wrapper', function() {
              // IL:
              const rows = dialog.$.dialog.querySelectorAll('.address-row');
              assertEquals(5 + experimental_fields_count, rows.length);

              // City, Postal code
              const row = rows[3 + experimental_fields_count]!;
              const cols =
                  row.querySelectorAll<CrTextareaElement|CrInputElement>(
                      '.address-column');
              assertEquals(2, cols.length);
              assertEquals(city, cols[0]!.value);
              assertEquals(zip, cols[1]!.value);

              countrySelect.value = 'US';
              countrySelect.dispatchEvent(new CustomEvent('change'));
            });
          })
          .then(function() {
            // US:
            const rows = dialog.$.dialog.querySelectorAll('.address-row');
            assertEquals(5 + experimental_fields_count, rows.length);

            // City, State, ZIP code
            const row = rows[3 + experimental_fields_count]!;
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
