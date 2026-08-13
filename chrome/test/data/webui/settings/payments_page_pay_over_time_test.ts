// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import type {SettingsPayOverTimeIssuerListEntryElement} from 'chrome://settings/lazy_load.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createPayOverTimeIssuerEntry} from './autofill_fake_data.js';
import {createPaymentsPage} from './payments_page_test_utils.js';

// clang-format on

suite('PaymentsPagePayOverTime', function() {
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      shouldShowPayOverTimeSettings: true,
      autofillEnableWalletBranding: true,
      autofillEnableGradientGoogleLogos: false,
    });
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  async function createPayOverTimeIssuerListEntry(
      issuer: chrome.autofillPrivate.PayOverTimeIssuerEntry):
      Promise<SettingsPayOverTimeIssuerListEntryElement> {
    const element =
        document.createElement('settings-pay-over-time-issuer-list-entry');
    element.payOverTimeIssuer = issuer;

    document.body.appendChild(element);
    await flushTasks();

    return element;
  }

  test('verifyPayOverTimeToggleIsShown', async function() {
    const page = await createPaymentsPage(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
          credit_card_enabled: {value: true},
        });
    const payOverTimeToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#payOverTimeToggle');

    assertTrue(!!payOverTimeToggle);
    assertEquals(
        loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
        payOverTimeToggle.label);
    assertEquals(
        loadTimeData.getString('autofillPayOverTimeSettingsSublabel'),
        payOverTimeToggle.subLabelWithLink);
  });

  test(
      'verifyPayOverTimeToggleIsNotShownWhenShouldShowPayOverTimeSettingsIsFalse',
      async function() {
        loadTimeData.overrideValues({
          shouldShowPayOverTimeSettings: false,
        });

        const page = await createPaymentsPage(
            /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
              credit_card_enabled: {value: true},
            });

        assertFalse(!!page.shadowRoot!.querySelector('#payOverTimeToggle'));
      });

  test(
      'verifyPayOverTimeToggleIsDisabledWhenCreditCardEnabledIsOff',
      async function() {
        const page = await createPaymentsPage(
            /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
              credit_card_enabled: {value: false},
            });
        const payOverTimeToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#payOverTimeToggle');

        assertTrue(!!payOverTimeToggle);
        assertTrue(payOverTimeToggle.disabled);
      });

  test('verifyPayOverTimeToggleSublabelLinkClickOpensUrl', async function() {
    const page = await createPaymentsPage(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
          credit_card_enabled: {value: true},
        });
    const payOverTimeToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#payOverTimeToggle');
    assertTrue(!!payOverTimeToggle);

    const link = payOverTimeToggle.shadowRoot!.querySelector('a');
    assertTrue(!!link);
    link.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('autofillPayOverTimeSettingsLearnMoreUrl'), url);
  });

  test('verifyPayOverTimePrefIsFalseWhenToggleIsOff', async function() {
    const page = await createPaymentsPage(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
          credit_card_enabled: {value: true},
          bnpl_enabled: {value: true},
        });
    const payOverTimeToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#payOverTimeToggle');
    assertTrue(!!payOverTimeToggle);
    assertTrue(payOverTimeToggle.checked);

    payOverTimeToggle.click();

    assertFalse(payOverTimeToggle.checked);
    assertFalse(payOverTimeToggle.pref!.value);
  });

  test('verifyPayOverTimeLinkToGPay', async function() {
    loadTimeData.overrideValues({
      autofillEnableWalletBranding: false,
    });

    const entry =
        await createPayOverTimeIssuerListEntry(createPayOverTimeIssuerEntry());

    const outlinkButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
    assertEquals('Your payment methods in Google Pay', outlinkButton.title);
    outlinkButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('managePaymentMethodsUrl'), url);
  });

  test('verifyPayOverTimeLinkToGoogleWallet', async function() {
    loadTimeData.overrideValues({
      autofillEnableWalletBranding: true,
    });

    const entry =
        await createPayOverTimeIssuerListEntry(createPayOverTimeIssuerEntry());

    const outlinkButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
    assertEquals('Your payment methods in Google Wallet', outlinkButton.title);
    outlinkButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('managePaymentMethodsUrl'), url);
  });

  test('verifyPayOverTimeIssuerSummaryLabel', async function() {
    const payOverTimeIssuer = createPayOverTimeIssuerEntry();
    payOverTimeIssuer.displayName = 'hello';

    const entry = await createPayOverTimeIssuerListEntry(payOverTimeIssuer);

    const payOverTimeItemSummaryLabel =
        entry.shadowRoot!.querySelector<HTMLElement>('#summaryLabel');

    assertTrue(!!payOverTimeItemSummaryLabel);
    assertEquals('hello', payOverTimeItemSummaryLabel.textContent.trim());
  });

  test('verifyGooglePayLogoWithGradient', async function() {
    loadTimeData.overrideValues({
      autofillEnableGradientGoogleLogos: true,
    });
    const payOverTimeIssuer = createPayOverTimeIssuerEntry();
    const entry = await createPayOverTimeIssuerListEntry(payOverTimeIssuer);
    const paymentsIcon = entry.shadowRoot!.querySelector('#paymentsIcon');
    // #paymentsIcon is only present in Google Chrome branded builds.
    if (paymentsIcon) {
      const source = paymentsIcon.querySelector('source');
      const img = paymentsIcon.querySelector('img');
      assertTrue(!!source);
      assertTrue(!!img);
      assertTrue(source.srcset.includes(
          'IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_DARK_SMALL'));
      assertTrue(
          img.srcset.includes('IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_SMALL'));
    } else {
      const textIndicator =
          entry.shadowRoot!.querySelector('#paymentsIndicator .sub-label');
      assertTrue(!!textIndicator);
      assertTrue(isVisible(textIndicator));
    }
  });

  test('verifyGooglePayLogoWithoutGradient', async function() {
    loadTimeData.overrideValues({
      autofillEnableGradientGoogleLogos: false,
    });
    const payOverTimeIssuer = createPayOverTimeIssuerEntry();
    const entry = await createPayOverTimeIssuerListEntry(payOverTimeIssuer);
    const paymentsIcon = entry.shadowRoot!.querySelector('#paymentsIcon');
    // #paymentsIcon is only present in Google Chrome branded builds.
    if (paymentsIcon) {
      const source = paymentsIcon.querySelector('source');
      const img = paymentsIcon.querySelector('img');
      assertTrue(!!source);
      assertTrue(!!img);
      assertTrue(source.srcset.includes('IDR_AUTOFILL_GOOGLE_PAY_DARK_SMALL'));
      assertTrue(img.srcset.includes('IDR_AUTOFILL_GOOGLE_PAY_SMALL'));
    } else {
      const textIndicator =
          entry.shadowRoot!.querySelector('#paymentsIndicator .sub-label');
      assertTrue(!!textIndicator);
      assertTrue(isVisible(textIndicator));
    }
  });
});
