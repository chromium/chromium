// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManagerImpl, CountryDetailManagerImpl} from 'chrome://settings/lazy_load.js';
import {AutofillManagerExpectations, createAddressEntry, createEmptyAddressEntry, TestAutofillManager} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {eventToPromise, whenAttributeIs} from 'chrome://test/test_util.m.js';
// clang-format on

/**
 * Test implementation.
 * @implements {CountryDetailManager}
 * @constructor
 */
function CountryDetailManagerTestImpl() {}

CountryDetailManagerTestImpl.prototype = {
  /** @override */
  getCountryList: function() {
    return new Promise(function(resolve) {
      resolve([
        {name: 'United States', countryCode: 'US'},  // Default test country.
        {name: 'Israel', countryCode: 'IL'},
        {name: 'United Kingdom', countryCode: 'GB'},
      ]);
    });
  },

  /** @override */
  getAddressFormat: function(countryCode) {
    return new Promise(function(resolve) {
      chrome.autofillPrivate.getAddressComponents(countryCode, resolve);
    });
  },
};

/**
 * Resolves the promise after the element fires the expected event. Will add
 * and remove the listener so it is only triggered once. |causeEvent| is
 * called after adding a listener to make sure that the event is captured.
 * @param {!Element} element
 * @param {string} eventName
 * @param {function():void} causeEvent
 * @return {!Promise}
 */
function expectEvent(element, eventName, causeEvent) {
  return new Promise(function(resolve) {
    const callback = function() {
      element.removeEventListener(eventName, callback);
      resolve.apply(this, arguments);
    };
    element.addEventListener(eventName, callback);
    causeEvent();
  });
}

/**
 * Creates the autofill section for the given list.
 * @param {!Array<!chrome.autofillPrivate.AddressEntry>} addresses
 * @param {!Object} prefValues
 * @return {!Object}
 */
function createAutofillSection(addresses, prefValues) {
  // Override the AutofillManagerImpl for testing.
  const autofillManager = new TestAutofillManager();
  autofillManager.data.addresses = addresses;
  AutofillManagerImpl.instance_ = autofillManager;

  const section = document.createElement('settings-autofill-section');
  section.prefs = {autofill: prefValues};
  document.body.appendChild(section);
  flush();

  return section;
}

/**
 * Creates the Edit Address dialog and fulfills the promise when the dialog
 * has actually opened.
 * @param {!chrome.autofillPrivate.AddressEntry} address
 * @return {!Promise<Object>}
 */
function createAddressDialog(address) {
  return new Promise(function(resolve) {
    const section = document.createElement('settings-address-edit-dialog');
    section.address = address;
    document.body.appendChild(section);
    eventToPromise('on-update-address-wrapper', section).then(function() {
      resolve(section);
    });
  });
}

/**
 * Creates the remove address dialog. Simulate clicking "Remove" button in
 * autofill section.
 * @param {!TestAutofillManager} autofillManager
 * @return {!SettingsAddressRemoveConfirmationDialogElement}
 */
function createRemoveAddressDialog(autofillManager) {
  const address = createAddressEntry();

  // Override the AutofillManagerImpl for testing.
  autofillManager.data.addresses = [address];
  AutofillManagerImpl.instance_ = autofillManager;

  document.body.innerHTML = '';
  const section = document.createElement('settings-autofill-section');
  document.body.appendChild(section);
  flush();

  const addressList = section.$.addressList;
  const row = addressList.children[0];
  assertTrue(!!row);

  // Simulate clicking the 'Remove' button in the menu.
  assertTrue(!!section.$$('#addressMenu'));
  section.$$('#addressMenu').click();
  flush();

  assertTrue(!!section.$$('#menuRemoveAddress'));
  assertFalse(!!section.$$('settings-address-remove-confirmation-dialog'));
  section.$$('#menuRemoveAddress').click();
  flush();

  assertTrue(!!section.$$('settings-address-remove-confirmation-dialog'));
  const removeAddressDialog =
      section.$$('settings-address-remove-confirmation-dialog');
  return removeAddressDialog;
}

suite('AutofillSectionUiTest', function() {
  test('testAutofillExtensionIndicator', function() {
    // Initializing with fake prefs
    const section = document.createElement('settings-autofill-section');
    section.prefs = {autofill: {profile_enabled: {}}};
    document.body.appendChild(section);

    assertFalse(!!section.$$('#autofillExtensionIndicator'));
    section.set('prefs.autofill.profile_enabled.extensionId', 'test-id');
    flush();

    assertTrue(!!section.$$('#autofillExtensionIndicator'));
  });
});

suite('AutofillSectionAddressTests', function() {
  suiteSetup(function() {
    CountryDetailManagerImpl.instance_ = new CountryDetailManagerTestImpl();
  });

  setup(function() {
    PolymerTest.clearBody();
  });

  /**
   * Will call |loopBody| for each item in |items|. Will only move to the next
   * item after the promise from |loopBody| resolves.
   * @param {!Array<Object>} items
   * @param {!function(!Object):!Promise} loopBody
   * @return {!Promise}
   */
  function asyncForEach(items, loopBody) {
    return new Promise(function(resolve) {
      let index = 0;

      function loop() {
        const item = items[index++];
        if (item) {
          loopBody(item).then(loop);
        } else {
          resolve();
        }
      }

      loop();
    });
  }

  test('verifyNoAddresses', function() {
    const section = createAutofillSection([], {profile_enabled: {value: true}});

    const addressList = section.$.addressList;
    assertTrue(!!addressList);
    // 1 for the template element.
    assertEquals(1, addressList.children.length);

    assertFalse(section.$.noAddressesLabel.hidden);
    assertFalse(section.$$('#addAddress').disabled);
    assertFalse(section.$$('#autofillProfileToggle').disabled);
  });

  test('verifyAddressCount', function() {
    const addresses = [
      createAddressEntry(),
      createAddressEntry(),
      createAddressEntry(),
      createAddressEntry(),
      createAddressEntry(),
    ];

    const section =
        createAutofillSection(addresses, {profile_enabled: {value: true}});

    const addressList = section.$.addressList;
    assertTrue(!!addressList);
    assertEquals(
        addresses.length, addressList.querySelectorAll('.list-item').length);

    assertTrue(section.$.noAddressesLabel.hidden);
    assertFalse(section.$$('#autofillProfileToggle').disabled);
    assertFalse(section.$$('#addAddress').disabled);
  });

  test('verifyAddressDisabled', function() {
    const section =
        createAutofillSection([], {profile_enabled: {value: false}});

    assertFalse(section.$$('#autofillProfileToggle').disabled);
    assertTrue(section.$$('#addAddress').hidden);
  });

  test('verifyAddressFields', function() {
    const address = createAddressEntry();
    const section = createAutofillSection([address], {});
    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);

    const addressSummary =
        address.metadata.summaryLabel + address.metadata.summarySublabel;

    let actualSummary = '';

    // Eliminate white space between nodes!
    const addressPieces = row.querySelector('#addressSummary').children;
    for (let i = 0; i < addressPieces.length; ++i) {
      actualSummary += addressPieces[i].textContent.trim();
    }

    assertEquals(addressSummary, actualSummary);
  });

  test('verifyAddressRowButtonTriggersDropdown', function() {
    const address = createAddressEntry();
    const section = createAutofillSection([address], {});
    const addressList = section.$.addressList;
    const row = addressList.children[0];
    assertTrue(!!row);
    const menuButton = row.querySelector('#addressMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertTrue(!!section.$$('#menuEditAddress'));
    assertTrue(!!section.$$('#menuRemoveAddress'));
  });

  test('verifyAddAddressDialog', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      const title = dialog.$$('[slot=title]');
      assertEquals(
          loadTimeData.getString('addAddressTitle'), title.textContent);
      // A country is preselected.
      assertTrue(!!address.countryCode);
    });
  });

  test('verifyEditAddressDialog', function() {
    return createAddressDialog(createAddressEntry()).then(function(dialog) {
      const title = dialog.$$('[slot=title]');
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
    const currentFocus = dialog.shadowRoot.activeElement;
    const editableElements =
        dialog.$.dialog.querySelectorAll('cr-input, select');
    assertEquals(editableElements[0], currentFocus);
  });

  test('verifyRemoveAddressDialogConfirmed', async function() {
    const autofillManager = new TestAutofillManager();
    const removeAddressDialog = createRemoveAddressDialog(autofillManager);

    // Wait for the dialog to open.
    await whenAttributeIs(removeAddressDialog.$$('#dialog'), 'open', '');

    assertTrue(!!removeAddressDialog.$$('#remove'));
    removeAddressDialog.$$('#remove').click();

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
    const removeAddressDialog = createRemoveAddressDialog(autofillManager);

    // Wait for the dialog to open.
    await whenAttributeIs(removeAddressDialog.$$('#dialog'), 'open', '');

    assertTrue(!!removeAddressDialog.$$('#cancel'));
    removeAddressDialog.$$('#cancel').click();

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
      const countrySelect = dialog.$$('select');
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

  test('verifyPhoneAndEmailAreSaved', function() {
    const address = createEmptyAddressEntry();
    return createAddressDialog(address).then(function(dialog) {
      assertEquals('', dialog.$.phoneInput.value);
      assertFalse(!!(address.phoneNumbers && address.phoneNumbers[0]));

      assertEquals('', dialog.$.emailInput.value);
      assertFalse(!!(address.emailAddresses && address.emailAddresses[0]));

      const phoneNumber = '(555) 555-5555';
      const emailAddress = 'no-reply@chromium.org';

      dialog.$.phoneInput.value = phoneNumber;
      dialog.$.emailInput.value = emailAddress;

      return expectEvent(dialog, 'save-address', function() {
               dialog.$.saveButton.click();
             }).then(function() {
        assertEquals(phoneNumber, dialog.$.phoneInput.value);
        assertEquals(phoneNumber, address.phoneNumbers[0]);

        assertEquals(emailAddress, dialog.$.emailInput.value);
        assertEquals(emailAddress, address.emailAddresses[0]);
      });
    });
  });

  test('verifyHonorificIsSaved', async function() {
    loadTimeData.overrideValues({showHonorific: true});
    const address = createEmptyAddressEntry();
    const dialog = await createAddressDialog(address);
    const honorificElement =
        dialog.$.dialog.querySelectorAll('settings-textarea, cr-input')[0];
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
    address.phoneNumbers = [phoneNumber];
    address.emailAddresses = [emailAddress];

    return createAddressDialog(address).then(function(dialog) {
      assertEquals(phoneNumber, dialog.$.phoneInput.value);
      assertEquals(emailAddress, dialog.$.emailInput.value);

      dialog.$.phoneInput.value = '';
      dialog.$.emailInput.value = '';

      return expectEvent(dialog, 'save-address', function() {
               dialog.$.saveButton.click();
             }).then(function() {
        assertEquals(0, address.phoneNumbers.length);
        assertEquals(0, address.emailAddresses.length);
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
        dialog.$.dialog.querySelectorAll('settings-textarea, cr-input');

    // The country can be preselected. Clear it to ensure the form is empty.
    await expectEvent(dialog, 'on-update-can-save', function() {
      const countrySelect = dialog.$$('select');
      countrySelect.value = '';
      countrySelect.dispatchEvent(new CustomEvent('change'));
    });

    // Default country is 'US' expecting: Honorific, Name, Organization,
    // Street address, City, State, ZIP code, Phone, and Email.
    // Unless Company name or honorific is disabled.
    const company_enabled = loadTimeData.getBoolean('EnableCompanyName');
    const honorific_enabled = loadTimeData.getBoolean('showHonorific');
    assertEquals(
        7 + (company_enabled ? 1 : 0) + (honorific_enabled ? 1 : 0),
        testElements.length);

    assertTrue(saveButton.disabled);
    await asyncForEach(testElements, async function(element) {
      await expectEvent(dialog, 'on-update-can-save', function() {
        element.value = 'foo';
      });
      assertFalse(saveButton.disabled);
      await expectEvent(dialog, 'on-update-can-save', function() {
        element.value = '';
      });
      assertTrue(saveButton.disabled);
    });
  });

  // Setting the country should allow the address to be saved.
  test('verifySaveIsNotClickableIfCountryNotSet', async function() {
    const simulateCountryChange = function(countryCode) {
      const countrySelect = dialog.$$('select');
      countrySelect.value = countryCode;
      countrySelect.dispatchEvent(new CustomEvent('change'));
    };

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
    address.fullNames = ['Name'];
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
});

suite('AutofillSectionAddressLocaleTests', function() {
  suiteSetup(function() {
    CountryDetailManagerImpl.instance_ = new CountryDetailManagerTestImpl();
  });

  setup(function() {
    PolymerTest.clearBody();
  });

  // US address has 3 fields on the same line.
  test('verifyEditingUSAddress', function() {
    const address = createEmptyAddressEntry();
    const company_enabled = loadTimeData.getBoolean('EnableCompanyName');
    const honorific_enabled = loadTimeData.getBoolean('showHonorific');

    address.honorific = 'Honorific';
    address.fullNames = ['Name'];
    address.companyName = 'Organization';
    address.addressLines = 'Street address';
    address.addressLevel2 = 'City';
    address.addressLevel1 = 'State';
    address.postalCode = 'ZIP code';
    address.countryCode = 'US';
    address.phoneNumbers = ['Phone'];
    address.emailAddresses = ['Email'];

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(
          5 + (company_enabled ? 1 : 0) + (honorific_enabled ? 1 : 0),
          rows.length);

      let index = 0;
      // Country
      let row = rows[index];
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'United States', countrySelect.selectedOptions[0].textContent.trim());
      index++;
      // Honorific
      if (honorific_enabled) {
        row = rows[index];
        const cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.honorific, cols[0].value);
        index++;
      }
      // Name
      row = rows[index];
      let cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.fullNames[0], cols[0].value);
      index++;
      // Organization
      if (company_enabled) {
        row = rows[index];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.companyName, cols[0].value);
        index++;
      }
      // Street address
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLines, cols[0].value);
      index++;
      // City, State, ZIP code
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(3, cols.length);
      assertEquals(address.addressLevel2, cols[0].value);
      assertEquals(address.addressLevel1, cols[1].value);
      assertEquals(address.postalCode, cols[2].value);
      index++;
      // Phone, Email
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.phoneNumbers[0], cols[0].value);
      assertEquals(address.emailAddresses[0], cols[1].value);
    });
  });

  // GB address has 1 field per line for all lines that change.
  test('verifyEditingGBAddress', function() {
    const address = createEmptyAddressEntry();
    const company_enabled = loadTimeData.getBoolean('EnableCompanyName');
    const honorific_enabled = loadTimeData.getBoolean('showHonorific');

    address.honorific = 'Lord';
    address.fullNames = ['Name'];
    address.companyName = 'Organization';
    address.addressLines = 'Street address';
    address.addressLevel2 = 'Post town';
    address.postalCode = 'Postal code';
    address.countryCode = 'GB';
    address.phoneNumbers = ['Phone'];
    address.emailAddresses = ['Email'];

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(
          6 + (company_enabled ? 1 : 0) + (honorific_enabled ? 1 : 0),
          rows.length);

      let index = 0;
      // Country
      let row = rows[index];
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'United Kingdom',
          countrySelect.selectedOptions[0].textContent.trim());
      index++;
      // Honorific
      if (honorific_enabled) {
        row = rows[index];
        const cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.honorific, cols[0].value);
        index++;
      }
      // Name
      row = rows[index];
      let cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.fullNames[0], cols[0].value);
      index++;
      // Organization
      if (company_enabled) {
        row = rows[index];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.companyName, cols[0].value);
        index++;
      }
      // Street address
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLines, cols[0].value);
      index++;
      // Post Town
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLevel2, cols[0].value);
      index++;
      // Postal code
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.postalCode, cols[0].value);
      index++;
      // Phone, Email
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.phoneNumbers[0], cols[0].value);
      assertEquals(address.emailAddresses[0], cols[1].value);
    });
  });

  // IL address has 2 fields on the same line and is an RTL locale.
  // RTL locale shouldn't affect this test.
  test('verifyEditingILAddress', function() {
    const address = createEmptyAddressEntry();
    const company_enabled = loadTimeData.getBoolean('EnableCompanyName');
    const honorific_enabled = loadTimeData.getBoolean('showHonorific');

    address.honorific = 'Honorific';
    address.fullNames = ['Name'];
    address.companyName = 'Organization';
    address.addressLines = 'Street address';
    address.addressLevel2 = 'City';
    address.postalCode = 'Postal code';
    address.countryCode = 'IL';
    address.phoneNumbers = ['Phone'];
    address.emailAddresses = ['Email'];

    return createAddressDialog(address).then(function(dialog) {
      const rows = dialog.$.dialog.querySelectorAll('.address-row');
      assertEquals(
          5 + (company_enabled ? 1 : 0) + (honorific_enabled ? 1 : 0),
          rows.length);

      let index = 0;
      // Country
      let row = rows[index];
      const countrySelect = row.querySelector('select');
      assertTrue(!!countrySelect);
      assertEquals(
          'Israel', countrySelect.selectedOptions[0].textContent.trim());
      index++;
      // Honorific
      if (honorific_enabled) {
        row = rows[index];
        const cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.honorific, cols[0].value);
        index++;
      }
      // Name
      row = rows[index];
      let cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.fullNames[0], cols[0].value);
      index++;
      // Organization
      if (company_enabled) {
        row = rows[index];
        cols = row.querySelectorAll('.address-column');
        assertEquals(1, cols.length);
        assertEquals(address.companyName, cols[0].value);
        index++;
      }
      // Street address
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(1, cols.length);
      assertEquals(address.addressLines, cols[0].value);
      index++;
      // City, Postal code
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.addressLevel2, cols[0].value);
      assertEquals(address.postalCode, cols[1].value);
      index++;
      // Phone, Email
      row = rows[index];
      cols = row.querySelectorAll('.address-column');
      assertEquals(2, cols.length);
      assertEquals(address.phoneNumbers[0], cols[0].value);
      assertEquals(address.emailAddresses[0], cols[1].value);
    });
  });

  // US has an extra field 'State'. Validate that this field is
  // persisted when switching to IL then back to US.
  test('verifyAddressPersistanceWhenSwitchingCountries', function() {
    const address = createEmptyAddressEntry();
    const company_enabled = loadTimeData.getBoolean('EnableCompanyName');
    const honorific_enabled = loadTimeData.getBoolean('showHonorific');
    const experimental_fields_count =
        (company_enabled ? 1 : 0) + (honorific_enabled ? 1 : 0);
    address.countryCode = 'US';

    return createAddressDialog(address).then(function(dialog) {
      const city = 'Los Angeles';
      const state = 'CA';
      const zip = '90291';
      const countrySelect = dialog.$$('select');

      return expectEvent(
                 dialog, 'on-update-address-wrapper',
                 function() {
                   // US:
                   const rows =
                       dialog.$.dialog.querySelectorAll('.address-row');
                   assertEquals(5 + experimental_fields_count, rows.length);

                   // City, State, ZIP code
                   const row = rows[3 + experimental_fields_count];
                   const cols = row.querySelectorAll('.address-column');
                   assertEquals(3, cols.length);
                   cols[0].value = city;
                   cols[1].value = state;
                   cols[2].value = zip;

                   countrySelect.value = 'IL';
                   countrySelect.dispatchEvent(new CustomEvent('change'));
                 })
          .then(function() {
            return expectEvent(dialog, 'on-update-address-wrapper', function() {
              // IL:
              const rows = dialog.$.dialog.querySelectorAll('.address-row');
              assertEquals(5 + experimental_fields_count, rows.length);

              // City, Postal code
              const row = rows[3 + experimental_fields_count];
              const cols = row.querySelectorAll('.address-column');
              assertEquals(2, cols.length);
              assertEquals(city, cols[0].value);
              assertEquals(zip, cols[1].value);

              countrySelect.value = 'US';
              countrySelect.dispatchEvent(new CustomEvent('change'));
            });
          })
          .then(function() {
            // US:
            const rows = dialog.$.dialog.querySelectorAll('.address-row');
            assertEquals(5 + experimental_fields_count, rows.length);

            // City, State, ZIP code
            const row = rows[3 + experimental_fields_count];
            const cols = row.querySelectorAll('.address-column');
            assertEquals(3, cols.length);
            assertEquals(city, cols[0].value);
            assertEquals(state, cols[1].value);
            assertEquals(zip, cols[2].value);
          });
    });
  });
});
