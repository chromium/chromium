// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ChromeCleanupProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, Router, routes, SafetyCheckCallbackConstants, SafetyCheckChromeCleanerStatus, SafetyCheckIconStatus, SafetyCheckInteractions, SettingsSafetyCheckChromeCleanerChildElement} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestChromeCleanupProxy} from './test_chrome_cleanup_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

const testDisplayString = 'Test display string';

/**
 * Fire a safety check Chrome cleaner event.
 */
function fireSafetyCheckChromeCleanerEvent(
    state: SafetyCheckChromeCleanerStatus) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(
      SafetyCheckCallbackConstants.CHROME_CLEANER_CHANGED, event);
}

interface AssertSafetyCheckChildParams {
  page: HTMLElement;
  iconStatus: SafetyCheckIconStatus;
  label: string;
  buttonLabel?: string;
  buttonAriaLabel?: string;
  buttonClass?: string;
  managedIcon?: boolean;
  rowClickable?: boolean;
}

/**
 * Verify that the safety check child inside the page has been configured as
 * specified.
 */
function assertSafetyCheckChild({
  page,
  iconStatus,
  label,
  buttonLabel,
  buttonAriaLabel,
  buttonClass,
  managedIcon,
  rowClickable,
}: AssertSafetyCheckChildParams) {
  const safetyCheckChild =
      page.shadowRoot!.querySelector('settings-safety-check-child');
  assertTrue(!!safetyCheckChild, 'safetyCheckChild is null');
  assertTrue(
      safetyCheckChild.iconStatus === iconStatus,
      'unexpected iconStatus: ' + safetyCheckChild.iconStatus);
  assertTrue(
      safetyCheckChild.label === label,
      'unexpected label: ' + safetyCheckChild.label);
  assertTrue(
      safetyCheckChild.subLabel === testDisplayString,
      'unexpected subLabel: ' + safetyCheckChild.subLabel);
  assertTrue(
      !buttonLabel || safetyCheckChild.buttonLabel === buttonLabel,
      'unexpected buttonLabel: ' + safetyCheckChild.buttonLabel);
  assertTrue(
      !buttonAriaLabel || safetyCheckChild.buttonAriaLabel === buttonAriaLabel,
      'unexpected buttonAriaLabel: ' + safetyCheckChild.buttonAriaLabel);
  assertTrue(
      !buttonClass || safetyCheckChild.buttonClass === buttonClass,
      'unexpected buttonClass: ' + safetyCheckChild.buttonClass);
  assertTrue(
      !!managedIcon === !!safetyCheckChild.managedIcon,
      'unexpected managedIcon: ' + safetyCheckChild.managedIcon);
  assertTrue(
      !!rowClickable === !!safetyCheckChild.rowClickable,
      'unexpected rowClickable: ' + safetyCheckChild.rowClickable);
}

suite('SafetyCheckChromeCleanerUiTests', function() {
  let chromeCleanupBrowserProxy: TestChromeCleanupProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsSafetyCheckChromeCleanerChildElement;

  setup(function() {
    chromeCleanupBrowserProxy = new TestChromeCleanupProxy();
    ChromeCleanupProxyImpl.setInstance(chromeCleanupBrowserProxy);

    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-chrome-cleaner-child');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  async function expectLogging(
      safetyCheckInteraction: SafetyCheckInteractions, userAction: string) {
    assertEquals(
        safetyCheckInteraction,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        userAction, await metricsBrowserProxy.whenCalled('recordAction'));
  }

  test('chromeCleanerHiddenUiTest', function() {
    fireSafetyCheckChromeCleanerEvent(SafetyCheckChromeCleanerStatus.HIDDEN);
    flush();
    // There is no Chrome cleaner child in safety check.
    assertFalse(
        !!page.shadowRoot!.querySelector('settings-safety-check-child'));
  });

  test('chromeCleanerCheckingUiTest', function() {
    fireSafetyCheckChromeCleanerEvent(SafetyCheckChromeCleanerStatus.CHECKING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Device software',
    });
  });

  test('chromeCleanerInfectedTest', async function() {
    fireSafetyCheckChromeCleanerEvent(SafetyCheckChromeCleanerStatus.INFECTED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Device software',
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review device software',
      buttonClass: 'action-button',
    });
    // User clicks the button.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.shadowRoot!
        .querySelector<HTMLElement>('#button')!.click();
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_REVIEW_INFECTED_STATE,
        'Settings.SafetyCheck.ChromeCleanerReviewInfectedState');
    // Ensure the correct Settings page is shown.
    assertEquals(routes.CHROME_CLEANUP, Router.getInstance().getCurrentRoute());
  });

  test('chromeCleanerRebootRequiredUiTest', async function() {
    fireSafetyCheckChromeCleanerEvent(
        SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Device software',
      buttonLabel: 'Restart computer',
      buttonAriaLabel: 'Restart computer',
      buttonClass: 'action-button',
    });
    // User clicks the button.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.shadowRoot!
        .querySelector<HTMLElement>('#button')!.click();
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_REBOOT,
        'Settings.SafetyCheck.ChromeCleanerReboot');
    // Ensure the browser proxy call is done.
    return chromeCleanupBrowserProxy.whenCalled('restartComputer');
  });

  test('chromeCleanerScanningForUwsUiTest', async function() {
    fireSafetyCheckChromeCleanerEvent(
        SafetyCheckChromeCleanerStatus.SCANNING_FOR_UWS);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Device software',
      rowClickable: true,
    });
    // User clicks the row.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.click();
    // Ensure UMA is logged.
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_CARET_NAVIGATION,
        'Settings.SafetyCheck.ChromeCleanerCaretNavigation');
    // Ensure the correct Settings page is shown.
    assertEquals(routes.CHROME_CLEANUP, Router.getInstance().getCurrentRoute());
  });

  test('chromeCleanerRemovingUwsUiTest', async function() {
    fireSafetyCheckChromeCleanerEvent(
        SafetyCheckChromeCleanerStatus.REMOVING_UWS);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Device software',
      rowClickable: true,
    });
    // User clicks the row.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.click();
    // Ensure UMA is logged.
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_CARET_NAVIGATION,
        'Settings.SafetyCheck.ChromeCleanerCaretNavigation');
    // Ensure the correct Settings page is shown.
    assertEquals(routes.CHROME_CLEANUP, Router.getInstance().getCurrentRoute());
  });

  test('chromeCleanerDisabledByAdminUiTest', function() {
    fireSafetyCheckChromeCleanerEvent(
        SafetyCheckChromeCleanerStatus.DISABLED_BY_ADMIN);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Device software',
      managedIcon: true,
    });
  });

  test('chromeCleanerErrorUiTest', async function() {
    fireSafetyCheckChromeCleanerEvent(SafetyCheckChromeCleanerStatus.ERROR);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Device software',
      rowClickable: true,
    });
    // User clicks the row.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.click();
    // Ensure UMA is logged.
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_CARET_NAVIGATION,
        'Settings.SafetyCheck.ChromeCleanerCaretNavigation');
    // Ensure the correct Settings page is shown.
    assertEquals(routes.CHROME_CLEANUP, Router.getInstance().getCurrentRoute());
  });

  test('chromeCleanerNoUwsFoundWithTimestampUiTest', async function() {
    fireSafetyCheckChromeCleanerEvent(
        SafetyCheckChromeCleanerStatus.NO_UWS_FOUND_WITH_TIMESTAMP);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Device software',
      rowClickable: true,
    });
    // User clicks the row.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.click();
    // Ensure UMA is logged.
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_CARET_NAVIGATION,
        'Settings.SafetyCheck.ChromeCleanerCaretNavigation');
    // Ensure the correct Settings page is shown.
    assertEquals(routes.CHROME_CLEANUP, Router.getInstance().getCurrentRoute());
  });

  test('chromeCleanerNoUwsFoundWithoutTimestampUiTest', async function() {
    fireSafetyCheckChromeCleanerEvent(
        SafetyCheckChromeCleanerStatus.NO_UWS_FOUND_WITHOUT_TIMESTAMP);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Device software',
      rowClickable: true,
    });
    // User clicks the row.
    page.shadowRoot!.querySelector('settings-safety-check-child')!.click();
    // Ensure UMA is logged.
    await expectLogging(
        SafetyCheckInteractions.CHROME_CLEANER_CARET_NAVIGATION,
        'Settings.SafetyCheck.ChromeCleanerCaretNavigation');
    // Ensure the correct Settings page is shown.
    assertEquals(routes.CHROME_CLEANUP, Router.getInstance().getCurrentRoute());
  });
});
