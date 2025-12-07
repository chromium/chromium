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

import {createPayOverTimeIssuerEntry} from './autofill_fake_data.js';
import {createPaymentsSection} from './payments_section_utils.js';

// clang-format on

suite('PaymentsSectionPayOverTime', function() {
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      shouldShowPayOverTimeSettings: true,
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
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
          credit_card_enabled: {value: true},
        });
    const payOverTimeToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#payOverTimeToggle');

    assertTrue(!!payOverTimeToggle);
    assertEquals(
        loadTimeData.getString('autofillPayOverTimeSettingsLabel'),
        payOverTimeToggle.label.toString());
    assertEquals(
        loadTimeData.getString('autofillPayOverTimeSettingsSublabel'),
        payOverTimeToggle.subLabelWithLink.toString());
  });

  test(
      'verifyPayOverTimeToggleIsNotShownWhenShouldShowPayOverTimeSettingsIsFalse',
      async function() {
        loadTimeData.overrideValues({
          shouldShowPayOverTimeSettings: false,
        });

        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
              credit_card_enabled: {value: true},
            });

        assertFalse(!!section.shadowRoot!.querySelector('#payOverTimeToggle'));
      });

  test(
      'verifyPayOverTimeToggleIsDisabledWhenCreditCardEnabledIsOff',
      async function() {
        const section = await createPaymentsSection(
            /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
              credit_card_enabled: {value: false},
            });
        const payOverTimeToggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#payOverTimeToggle');

        assertTrue(!!payOverTimeToggle);
        assertTrue(payOverTimeToggle.disabled);
      });

  test('verifyPayOverTimeToggleSublabelLinkClickOpensUrl', async function() {
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
          credit_card_enabled: {value: true},
        });
    const payOverTimeToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
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
    const section = await createPaymentsSection(
        /*creditCards=*/[], /*ibans=*/[], /*payOverTimeIssuers=*/[], {
          credit_card_enabled: {value: true},
          bnpl_enabled: {value: true},
        });
    const payOverTimeToggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#payOverTimeToggle');
    assertTrue(!!payOverTimeToggle);
    assertTrue(payOverTimeToggle.checked);

    payOverTimeToggle.click();

    assertFalse(payOverTimeToggle.checked);
    assertFalse(payOverTimeToggle.pref!.value);
  });

  test('verifyPayOverTimeLinkToGPay', async function() {
    const entry =
        await createPayOverTimeIssuerListEntry(createPayOverTimeIssuerEntry());

    const outlinkButton = entry.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
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
});
