// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {AutofillManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestAutofillManager} from './autofill_fake_data.js';
import {createAddressEntry} from './autofill_fake_data.js';
import {createContactInfoPage, deleteAddress} from './contact_info_page_test_utils.js';

import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

suite('ContactInfoPageFocusTest', function() {
  test('verifyFocusLocationAfterRemoving', async () => {
    const page = await createContactInfoPage(
        [
          createAddressEntry(),
          createAddressEntry(),
          createAddressEntry(),
        ],
        {profile_enabled: {value: true}});

    // Ensure the subpage's back button is focused before continuing further.
    await waitAfterNextRender(page);
    const subpageElement =
        page.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!subpageElement);
    // Note: Using assertTrue instead of assertEquals on purpose, because Mocha
    // in case of failure tries to serialize the arguments, which in turn throws
    // 'TypeError: Converting circular structure to JSON' instead of surfacing
    // the assertion error.
    assertTrue(subpageElement.$.closeButton === getDeepActiveElement());

    const manager = AutofillManagerImpl.getInstance() as TestAutofillManager;

    await deleteAddress(page, manager, 1);
    const addressesAfterRemovingInTheMiddle =
        page.$.addressList.querySelectorAll('.list-item');
    assertTrue(
        addressesAfterRemovingInTheMiddle[1]!.matches(':focus-within'),
        'The focus should remain on the same index on the list (but next ' +
            'to the removed address).');

    await deleteAddress(page, manager, 1);
    const addressesAfterRemovingLastInTheList =
        page.$.addressList.querySelectorAll('.list-item');
    assertTrue(
        addressesAfterRemovingLastInTheList[0]!.matches(':focus-within'),
        'After removing the last address on the list the focus should go ' +
            'to the preivous address.');

    await deleteAddress(page, manager, 0);
    assertTrue(
        page.$.addAddress.matches(':focus-within'),
        'If there are no addresses remaining after removal the focus should ' +
            'go to the Add button.');

    document.body.removeChild(page);
  });
});
