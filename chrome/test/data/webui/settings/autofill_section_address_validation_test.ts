// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {CrInputElement, CrTextareaElement} from 'chrome://settings/lazy_load.js';
import {CountryDetailManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createAddressEntry, createEmptyAddressEntry, makeGuid, STUB_USER_ACCOUNT_INFO} from './autofill_fake_data.js';
import {createAddressDialog, expectEvent} from './autofill_section_test_utils.js';
import {TestCountryDetailManagerProxy} from './test_country_detail_manager_proxy.js';
// clang-format on

const FieldType = chrome.autofillPrivate.FieldType;

suite('AutofillSectionAddressValidationTests', () => {
  let countryDetailManager: TestCountryDetailManagerProxy;

  setup(() => {
    countryDetailManager = new TestCountryDetailManagerProxy();
    CountryDetailManagerProxyImpl.setInstance(countryDetailManager);

    countryDetailManager.setGetCountryListRepsonse([
      {name: 'United States', countryCode: 'US'},  // Default country.
      {name: 'Israel', countryCode: 'IL'},
      {name: 'United Kingdom', countryCode: 'GB'},
    ]);
    countryDetailManager.setGetAddressFormatRepsonse({
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
          row: [{
            field: FieldType.COMPANY_NAME,
            fieldName: 'Organization',
            isLongField: true,
            isRequired: false,
          }],
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
    });
  });

  test('verifyRequiredFields', async () => {
    const address = createEmptyAddressEntry();
    address.fields.push({type: FieldType.ADDRESS_HOME_COUNTRY, value: 'US'});

    const dialog = await createAddressDialog(address, {
      ...STUB_USER_ACCOUNT_INFO,
      isEligibleForAddressAccountStorage: true,
    });
    const content = dialog.$.dialog;
    const save = dialog.$.saveButton;

    assertEquals(
        3, content.querySelectorAll('[required]').length,
        'number of required elements should match required components');
    assertEquals(
        0, content.querySelectorAll('[invalid]').length,
        'initial state, no invalid elements indication');
    assertFalse(
        save.disabled, 'initial state, the save button should be enabled');

    await expectEvent(dialog, 'on-update-can-save', () => save.click());

    // Attempt to save reveals invalid fields, the address is not saveable.
    assertEquals(
        3, content.querySelectorAll('[invalid]').length,
        'all required components should be indicated after an attempt to save');
    assertTrue(
        save.disabled, 'the save button is disable with revealed errors');
  });


  test('verifyClearingOutRequiredField', async () => {
    const dialog = await createAddressDialog(createEmptyAddressEntry(), {
      ...STUB_USER_ACCOUNT_INFO,
      isEligibleForAddressAccountStorage: true,
    });
    const content = dialog.$.dialog;
    const save = dialog.$.saveButton;
    const requiredElements =
        content.querySelectorAll<CrTextareaElement|CrInputElement>(
            '[required]');

    assertGT(requiredElements.length, 1);

    const firstRequired = requiredElements[0]!;
    assertFalse(firstRequired.invalid, 'no error on empty element initially');
    // Imitate typing and clearing in the input.
    firstRequired.value = 'value';
    await microtasksFinished();
    firstRequired.value = '';
    await microtasksFinished();
    assertTrue(firstRequired.invalid, 'indicate empty element after updates');


    assertFalse(
        save.disabled,
        'visual invalidation of one field does not affect the save button state\
         as there are other fields whose invalid state is not revealed yet');
  });

  test('verifyFormSaveability', async () => {
    const dialog = await createAddressDialog(createEmptyAddressEntry(), {
      ...STUB_USER_ACCOUNT_INFO,
      isEligibleForAddressAccountStorage: true,
    });
    const content = dialog.$.dialog;
    const save = dialog.$.saveButton;
    const requiredElements =
        content.querySelectorAll<CrTextareaElement|CrInputElement>(
            '[required]');

    // Reveal errors, disable the save button.
    await expectEvent(dialog, 'on-update-can-save', () => save.click());

    for (const required of requiredElements) {
      assertTrue(
          save.disabled,
          'the save button should be disabled until all requirements are\
           satisfied');
      await expectEvent(
          dialog, 'on-update-can-save',
          () => required.value = 'something typed');
    }

    assertFalse(
        save.disabled,
        'visual invalidation of one field does not affect the save button state\
         as there are other fields whose invalid state is not revealed yet');
  });

  test('verifySaveabilityResetOnCountryChange', async () => {
    const dialog = await createAddressDialog(createEmptyAddressEntry(), {
      ...STUB_USER_ACCOUNT_INFO,
      isEligibleForAddressAccountStorage: true,
    });
    const content = dialog.$.dialog;
    const save = dialog.$.saveButton;
    const country = dialog.$.country;

    // Reveal errors by trying to save.
    await expectEvent(dialog, 'on-update-can-save', () => save.click());
    assertGT(
        content.querySelectorAll('[invalid]').length, 0,
        'invalid elements should be revealed');
    assertTrue(
        save.disabled, 'the save button is disabled with all errors revealed');

    // Change country.
    await expectEvent(dialog, 'on-update-can-save', () => {
      country.value = 'GB';
      country.dispatchEvent(new CustomEvent('change'));
    });

    assertEquals(
        0, content.querySelectorAll('[invalid]').length,
        'country change should reset invalid elements');
    assertFalse(save.disabled, 'country change should reset form saveability');
  });

  test('verifySaveabilityOfInitiallyInvalid', async () => {
    const address = createAddressEntry();
    address.metadata!.recordType =
        chrome.autofillPrivate.AddressRecordType.ACCOUNT;

    // This field is required.
    const entry = address.fields.find(
        entry => entry.type === FieldType.ADDRESS_HOME_STREET_ADDRESS);
    assertTrue(!!entry);
    address.fields.splice(address.fields.indexOf(entry), 1);

    const dialog = await createAddressDialog(address);
    const save = dialog.$.saveButton;

    await expectEvent(dialog, 'save-address', () => save.click());
  });

  test('verifyInvalidatingInitiallyInvalid', async () => {
    const address = createEmptyAddressEntry();
    address.guid = makeGuid();
    address.fields.push({type: FieldType.ADDRESS_HOME_COUNTRY, value: 'US'});
    address.metadata = {
      summaryLabel: '',
      recordType: chrome.autofillPrivate.AddressRecordType.ACCOUNT,
    };

    const dialog = await createAddressDialog(address);
    const content = dialog.$.dialog;
    const save = dialog.$.saveButton;
    const requiredElements =
        content.querySelectorAll<CrTextareaElement|CrInputElement>(
            '[required]');

    assertGT(requiredElements.length, 1);

    for (const required of requiredElements) {
      await expectEvent(dialog, 'on-update-can-save', () => {
        // Imitate typing and clearing in the input.
        required.value = 'something typed';
        required.value = '';
      });
      assertFalse(
          required.invalid,
          'initially invalid component should not be invalidated');
      assertFalse(
          save.disabled,
          'initially invalid component does not affect saveability');
    }

    await expectEvent(dialog, 'save-address', () => save.click());
  });
});
