// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createCreditCardEntry, createIbanEntry} from './autofill_fake_data.js';
import {createPaymentsSection, getPaymentMethodEntry, PaymentMethod, deletePaymentMethod} from './payments_section_utils.js';
// clang-format on

suite('PaymentSectionFocusTests', function() {
  setup(function() {
    loadTimeData.overrideValues({
      showIbansSettings: true,
    });
  });

  test('FocusLocationAfterDeletion', async function() {
    const section = await createPaymentsSection(
        [
          createCreditCardEntry(),
          createCreditCardEntry(),
        ],
        [
          createIbanEntry('FI1410093000123458', 'NickName'),
        ],
        {credit_card_enabled: {value: true}});
    const manager = (PaymentsManagerImpl.getInstance() as TestPaymentsManager);

    const addButton = section.shadowRoot!.querySelector('#addPaymentMethods');
    assertTrue(!!addButton);

    await deletePaymentMethod(section, manager, PaymentMethod.CREDIT_CARD, 1);
    assertTrue(
        getPaymentMethodEntry(section, 'iban-0').matches(':focus-within'),
        'The focus should go to the first IBAN entry after removing ' +
            'the latest credit card.');

    await deletePaymentMethod(section, manager, PaymentMethod.IBAN, 0);
    assertTrue(
        getPaymentMethodEntry(section, 'card-0').matches(':focus-within'),
        'The focus should be set on the preceding entry as the removed IBAN ' +
            'was the latest payment method in the list.');

    await deletePaymentMethod(section, manager, PaymentMethod.CREDIT_CARD, 0);
    assertTrue(
        addButton.matches(':focus-within'),
        'No payment methods in the list, the focus goes to the Add button');
  });
});
