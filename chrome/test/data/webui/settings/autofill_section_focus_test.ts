// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {AutofillManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestAutofillManager} from './autofill_fake_data.js';
import {createAddressEntry} from './autofill_fake_data.js';
import {createAutofillSection, deleteAddress} from './autofill_section_test_utils.js';
// clang-format on

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
