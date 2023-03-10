// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createPaymentsSection} from './payments_section_utils.js';

// clang-format on

suite('PaymentsSectionUpi', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      removeCardExpirationAndTypeTitles: true,
      virtualCardEnrollmentEnabled: true,
      showIbansSettings: true,
    });
  });

  /**
   * Returns the shadow root of the UPI ID row from the specified list of
   * payment methods.
   */
  function getUPIRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
    const row =
        paymentsList.shadowRoot!.querySelector('settings-upi-id-list-entry');
    assertTrue(!!row);
    return row.shadowRoot!;
  }

  test('verifyUpiIdRow', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], ['vpa@indianbank'],
        /*prefValues=*/ {});
    const rowShadowRoot = getUPIRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot);
    assertEquals(
        rowShadowRoot.querySelector<HTMLElement>('#upiIdLabel')!.textContent,
        'vpa@indianbank');
  });

  test('verifyNoUpiId', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('verifyUpiIdCount', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const upiIds = ['vpa1@indianbank', 'vpa2@indianbank'];
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(upiIds.length, upiRows.length);
  });

  // Test that |showUpiIdSettings| controls showing UPI IDs in the page.
  test('verifyShowUpiIdSettings', async function() {
    loadTimeData.overrideValues({showUpiIdSettings: false});

    const upiIds = ['vpa1@indianbank'];
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });
});
