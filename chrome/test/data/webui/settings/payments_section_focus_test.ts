// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestPaymentsManager} from './autofill_fake_data.js';
import {createCreditCardEntry, createIbanEntry} from './autofill_fake_data.js';
import {createPaymentsSection, getPaymentMethodEntry, PaymentMethod, deletePaymentMethod} from './payments_section_utils.js';

import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
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
        /*payOverTimeIssuers=*/[], {credit_card_enabled: {value: true}});

    // Ensure the subpage's back button is focused before continuing further.
    await waitAfterNextRender(section);
    const subpageElement =
        section.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!subpageElement);
    // Note: Using assertTrue instead of assertEquals on purpose, because Mocha
    // in case of failure tries to serialize the arguments, which in turn throws
    // 'TypeError: Converting circular structure to JSON' instead of surfacing
    // the assertion error.
    assertTrue(subpageElement.$.closeButton === getDeepActiveElement());

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
