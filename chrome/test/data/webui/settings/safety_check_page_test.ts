// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SafetyCheckExtensionsElement, SafetyCheckBrowserProxy, SettingsSafetyCheckChildElement, SettingsSafetyCheckExtensionsChildElement, SettingsSafetyCheckPageElement, SettingsSafetyCheckPasswordsChildElement, SettingsSafetyCheckSafeBrowsingChildElement ,SettingsSafetyCheckUpdatesChildElement} from 'chrome://settings/settings.js';
import {SafetyCheckExtensionsBrowserProxyImpl, HatsBrowserProxyImpl, LifetimeBrowserProxyImpl, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PasswordCheckReferrer, PasswordManagerImpl, Router, routes, PasswordManagerPage, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus, SafetyCheckIconStatus, SafetyCheckInteractions, SafetyCheckParentStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {TestExtensionBrowserProxy} from './test_extensions_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {assertSafetyCheckChild} from './safety_check_test_utils.js';

// clang-format on

const testDisplayString = 'Test display string';
const passwordsString = 'Password Manager';

/**
 * Fire a safety check parent event.
 */
function fireSafetyCheckParentEvent(state: SafetyCheckParentStatus) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.PARENT_CHANGED, event);
}

/**
 * Fire a safety check updates event.
 */
function fireSafetyCheckUpdatesEvent(state: SafetyCheckUpdatesStatus) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.UPDATES_CHANGED, event);
}

/**
 * Fire a safety check passwords event.
 */
function fireSafetyCheckPasswordsEvent(state: SafetyCheckPasswordsStatus) {
  const event = {
    newState: state,
    displayString: testDisplayString,
    passwordsButtonString: null,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.PASSWORDS_CHANGED, event);
}

/**
 * Fire a safety check safe browsing event.
 */
function fireSafetyCheckSafeBrowsingEvent(
    state: SafetyCheckSafeBrowsingStatus) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(
      SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED, event);
}

/**
 * Fire a safety check extensions event.
 */
function fireSafetyCheckExtensionsEvent(state: SafetyCheckExtensionsStatus) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.EXTENSIONS_CHANGED, event);
}

class TestSafetyCheckBrowserProxy extends TestBrowserProxy implements
    SafetyCheckBrowserProxy {
  private parentRanDisplayString_ = '';

  constructor() {
    super([
      'getParentRanDisplayString',
      'runSafetyCheck',
    ]);
  }

  runSafetyCheck() {
    this.methodCalled('runSafetyCheck');
  }

  setParentRanDisplayString(s: string) {
    this.parentRanDisplayString_ = s;
  }

  getParentRanDisplayString() {
    this.methodCalled('getParentRanDisplayString');
    return Promise.resolve(this.parentRanDisplayString_);
  }
}

suite('SafetyCheckPageUiTests', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let safetyCheckBrowserProxy: TestSafetyCheckBrowserProxy;
  let page: SettingsSafetyCheckPageElement;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    safetyCheckBrowserProxy = new TestSafetyCheckBrowserProxy();
    safetyCheckBrowserProxy.setParentRanDisplayString('Dummy string');
    SafetyCheckBrowserProxyImpl.setInstance(safetyCheckBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-page');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  /** Tests parent element and collapse.from start to completion */
  test('testParentAndCollapse', async function() {
    // Before the check, only the text button is present.
    assertTrue(!!page.shadowRoot!.querySelector('#safetyCheckParentButton'));
    assertFalse(!!page.shadowRoot!.querySelector('cr-icon-button'));
    // Collapse is not opened.
    const collapse = page.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(collapse.opened);

    // User starts check.
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#safetyCheckParentButton')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.RUN_SAFETY_CHECK,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.Start',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    await safetyCheckBrowserProxy.whenCalled('runSafetyCheck');

    // Mock all incoming messages that indicate safety check is running.
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.CHECKING);
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.CHECKING);
    fireSafetyCheckSafeBrowsingEvent(SafetyCheckSafeBrowsingStatus.CHECKING);
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.CHECKING);
    fireSafetyCheckParentEvent(SafetyCheckParentStatus.CHECKING);

    flush();
    // Only the icon button is present.
    assertFalse(!!page.shadowRoot!.querySelector('#safetyCheckParentButton'));
    assertTrue(!!page.shadowRoot!.querySelector('cr-icon-button'));
    // Collapse is opened.
    assertTrue(collapse.opened);

    // Mock all incoming messages that indicate safety check completion.
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATED);
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.SAFE);
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD);
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);
    fireSafetyCheckParentEvent(SafetyCheckParentStatus.AFTER);

    flush();
    // Only the icon button is present.
    assertFalse(!!page.shadowRoot!.querySelector('#safetyCheckParentButton'));
    assertTrue(!!page.shadowRoot!.querySelector('cr-icon-button'));
    // Collapse is opened.
    assertTrue(page.shadowRoot!.querySelector('cr-collapse')!.opened);

    // Ensure the automatic browser proxy calls are started.
    return safetyCheckBrowserProxy.whenCalled('getParentRanDisplayString');
  });

  test('HappinessTrackingSurveysTest', async function() {
    const testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#safetyCheckParentButton')!.click();
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.RAN_SAFETY_CHECK, interaction);
  });
});

suite('SafetyCheckChildTests', function() {
  let page: SettingsSafetyCheckChildElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-child');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('testIconStatusRunning', function() {
    page.iconStatus = SafetyCheckIconStatus.RUNNING;
    flush();
    const statusIconElem =
        page.shadowRoot!.querySelector<HTMLElement>('#statusIcon');
    assertTrue(!!statusIconElem);
    assertTrue(statusIconElem!.classList.contains('icon-blue'));
    assertFalse(statusIconElem!.classList.contains('icon-red'));
    assertEquals('Running', statusIconElem!.getAttribute('aria-label'));
  });

  test('testIconStatusSafe', function() {
    page.iconStatus = SafetyCheckIconStatus.SAFE;
    flush();
    const statusIconElem =
        page.shadowRoot!.querySelector<HTMLElement>('#statusIcon');
    assertTrue(!!statusIconElem);
    assertTrue(statusIconElem!.classList.contains('icon-blue'));
    assertFalse(statusIconElem!.classList.contains('icon-red'));
    assertEquals('Passed', statusIconElem!.getAttribute('aria-label'));
  });

  test('testIconStatusInfo', function() {
    page.iconStatus = SafetyCheckIconStatus.INFO;
    flush();
    const statusIconElem =
        page.shadowRoot!.querySelector<HTMLElement>('#statusIcon');
    assertTrue(!!statusIconElem);
    assertFalse(statusIconElem!.classList.contains('icon-blue'));
    assertFalse(statusIconElem!.classList.contains('icon-red'));
    assertEquals('Info', statusIconElem!.getAttribute('aria-label'));
  });

  test('testIconStatusWarning', function() {
    page.iconStatus = SafetyCheckIconStatus.WARNING;
    flush();
    const statusIconElem =
        page.shadowRoot!.querySelector<HTMLElement>('#statusIcon');
    assertTrue(!!statusIconElem);
    assertFalse(statusIconElem!.classList.contains('icon-blue'));
    assertTrue(statusIconElem!.classList.contains('icon-red'));
    assertEquals('Warning', statusIconElem!.getAttribute('aria-label'));
  });

  test('testLabelText', function() {
    page.label = 'Main label test text';
    flush();
    const label = page.shadowRoot!.querySelector('#label');
    assertTrue(!!label);
    assertEquals('Main label test text', label!.textContent!.trim());
  });

  test('testSubLabelText', function() {
    page.subLabel = 'Sub label test text';
    flush();
    const subLabel = page.shadowRoot!.querySelector('#subLabel');
    assertTrue(!!subLabel);
    assertEquals('Sub label test text', subLabel!.textContent!.trim());
  });

  test('testSubLabelNoText', function() {
    // sublabel not set -> empty sublabel in element
    const subLabel = page.shadowRoot!.querySelector('#subLabel');
    assertTrue(!!subLabel);
    assertEquals('', subLabel!.textContent!.trim());
  });

  test('testButtonWithoutClass', function() {
    page.buttonLabel = 'Button label';
    page.buttonAriaLabel = 'Aria label';
    flush();
    const button = page.shadowRoot!.querySelector('#button');
    assertTrue(!!button);
    assertEquals('Button label', button!.textContent!.trim());
    assertEquals('Aria label', button!.getAttribute('aria-label'));
    assertFalse(button!.classList.contains('action-button'));
  });

  test('testButtonWithClass', function() {
    page.buttonLabel = 'Button label';
    page.buttonAriaLabel = 'Aria label';
    page.buttonClass = 'action-button';
    flush();
    const button = page.shadowRoot!.querySelector('#button');
    assertTrue(!!button);
    assertEquals('Button label', button!.textContent!.trim());
    assertEquals('Aria label', button!.getAttribute('aria-label'));
    assertTrue(button!.classList.contains('action-button'));
  });

  test('testNoButton', function() {
    // Button label not set -> no button.
    assertFalse(!!page.shadowRoot!.querySelector('#button'));
  });

  test('testManagedIcon', function() {
    page.managedIcon = 'cr20:domain';
    flush();
    assertTrue(!!page.shadowRoot!.querySelector('#managedIcon'));
  });

  test('testNoManagedIcon', function() {
    // Managed icon not set -> no managed icon.
    assertFalse(!!page.shadowRoot!.querySelector('#managedIcon'));
  });

  test('testRowClickableIndicator', function() {
    page.rowClickable = true;
    flush();
    assertTrue(!!page.shadowRoot!.querySelector('#rowClickableIndicator'));
    assertEquals(
        'cr:arrow-right',
        page.shadowRoot!.querySelector('#rowClickableIndicator')!.getAttribute(
            'iron-icon'));
  });

  test('testExternalRowClickableIndicator', function() {
    page.rowClickable = true;
    page.external = true;
    flush();
    assertTrue(!!page.shadowRoot!.querySelector('#rowClickableIndicator'));
    assertEquals(
        'cr:open-in-new',
        page.shadowRoot!.querySelector('#rowClickableIndicator')!.getAttribute(
            'iron-icon'));
  });

  test('testNoRowClickableIndicator', function() {
    // rowClickable not set -> no RowClickableIndicator.
    assertFalse(!!page.shadowRoot!.querySelector('#rowClickableIndicator'));
  });
});

suite('SafetyCheckUpdatesChildUiTests', function() {
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsSafetyCheckUpdatesChildElement;

  setup(function() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-updates-child');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('checkingUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.CHECKING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });

  test('updatedUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });

  test('updatingUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });

  test('relaunchUiTest', async function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.RELAUNCH);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
      sublabel: testDisplayString,
      buttonLabel: page.i18n('aboutRelaunch'),
      buttonAriaLabel: page.i18n('safetyCheckUpdatesButtonAriaLabel'),
      buttonClass: 'action-button',
    });

    // User clicks the relaunch button.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>('#safetyCheckChild')!
        .shadowRoot!.querySelector<HTMLElement>('#button')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.UPDATES_RELAUNCH,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.RelaunchAfterUpdates',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('disabledByAdminUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
      sublabel: testDisplayString,
      managedIcon: true,
    });
  });

  test('failedOfflineUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.FAILED_OFFLINE);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });

  test('failedUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.FAILED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });

  test('unknownUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UNKNOWN);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });

  test('updateToRollbackVersionDisallowedUiTest', () => {
    fireSafetyCheckUpdatesEvent(
        SafetyCheckUpdatesStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED);
    flush();
    assertSafetyCheckChild({
      page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
      sublabel: testDisplayString,
    });
  });
});

suite('SafetyCheckPasswordsChildUiTests', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let passwordManager: TestPasswordManagerProxy;
  let page: SettingsSafetyCheckPasswordsChildElement;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-passwords-child');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('passwordCheckingUiTest', function() {
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.CHECKING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: passwordsString,
      sublabel: testDisplayString,
    });
  });

  test('passwordSafeUiTest', async function() {
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.SAFE);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: passwordsString,
      sublabel: testDisplayString,
      rowClickable: true,
    });

    // User clicks the row.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>(
            '#safetyCheckChild')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.PASSWORDS_CARET_NAVIGATION,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManagePasswordsThroughCaretNavigation',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the Password Check page is shown.
    const param = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);
  });

  test('passwordCompromisedUiTest', async function() {
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.COMPROMISED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: passwordsString,
      sublabel: testDisplayString,
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review passwords',
      buttonClass: 'action-button',
    });

    const passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    // User clicks the manage passwords button.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>('#safetyCheckChild')!
        .shadowRoot!.querySelector<HTMLElement>('#button')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.PASSWORDS_MANAGE_COMPROMISED_PASSWORDS,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManagePasswords',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the Password Check page is shown.
    const param = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);

    // Ensure correct referrer sent to password check.
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(PasswordCheckReferrer.SAFETY_CHECK, referrer);
  });

  test('passwordWeakUiTest', async function() {
    fireSafetyCheckPasswordsEvent(
        SafetyCheckPasswordsStatus.WEAK_PASSWORDS_EXIST);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: passwordsString,
      sublabel: testDisplayString,
      rowClickable: true,
    });

    // User clicks the manage passwords button.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>(
            '#safetyCheckChild')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.PASSWORDS_MANAGE_WEAK_PASSWORDS,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManageWeakPasswords',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the Password Check page is shown.
    const param = await passwordManager.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);
  });

  test('passwordInfoStatesUiTest', function() {
    // Iterate over all states
    for (const state of Object.values(SafetyCheckPasswordsStatus)
             .filter(v => Number.isInteger(v))) {
      fireSafetyCheckPasswordsEvent(state as SafetyCheckPasswordsStatus);
      flush();

      // Check that icon status is the correct one for this password status.
      switch (state) {
        case SafetyCheckPasswordsStatus.OFFLINE:
        case SafetyCheckPasswordsStatus.NO_PASSWORDS:
        case SafetyCheckPasswordsStatus.SIGNED_OUT:
        case SafetyCheckPasswordsStatus.FEATURE_UNAVAILABLE:
          assertSafetyCheckChild({
            page: page,
            iconStatus: SafetyCheckIconStatus.INFO,
            label: passwordsString,
            sublabel: testDisplayString,
          });
          break;
        case SafetyCheckPasswordsStatus.QUOTA_LIMIT:
        case SafetyCheckPasswordsStatus.ERROR:
          assertSafetyCheckChild({
            page: page,
            iconStatus: SafetyCheckIconStatus.INFO,
            label: passwordsString,
            sublabel: testDisplayString,
            rowClickable: true,
          });
          break;
        default:
          // Not covered by this test.
          break;
      }
    }
  });
});

suite('SafetyCheckSafeBrowsingChildUiTests', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsSafetyCheckSafeBrowsingChildElement;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-safe-browsing-child');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('safeBrowsingCheckingUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(SafetyCheckSafeBrowsingStatus.CHECKING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
    });
  });

  test('safeBrowsingEnabledStandardUiTest', async function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
      rowClickable: true,
    });

    // User clicks the row.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>(
            '#safetyCheckChild')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFE_BROWSING_CARET_NAVIGATION,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManageSafeBrowsingThroughCaretNavigation',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });

  test('safeBrowsingEnabledStandardAvailableEnhancedUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD_AVAILABLE_ENHANCED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
      rowClickable: true,
    });
  });

  test('safeBrowsingEnabledEnhancedUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_ENHANCED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
      rowClickable: true,
    });
  });

  test('safeBrowsingDisabledUiTest', async function() {
    fireSafetyCheckSafeBrowsingEvent(SafetyCheckSafeBrowsingStatus.DISABLED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
      buttonLabel: 'Manage',
      buttonAriaLabel: 'Manage Safe Browsing',
      buttonClass: 'action-button',
    });

    // User clicks the manage safe browsing button.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>('#safetyCheckChild')!
        .shadowRoot!.querySelector<HTMLElement>('#button')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.SAFE_BROWSING_MANAGE,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManageSafeBrowsing',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });

  test('safeBrowsingDisabledByAdminUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
      managedIcon: true,
      rowClickable: true,
    });
  });

  test('safeBrowsingDisabledByExtensionUiTest', function() {
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Safe Browsing',
      sublabel: testDisplayString,
      managedIcon: true,
      rowClickable: true,
    });
  });
});

suite('SafetyCheckExtensionsUiTests', function() {
  let page: SafetyCheckExtensionsElement;
  let browserProxy: TestExtensionBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    browserProxy = new TestExtensionBrowserProxy();
    SafetyCheckExtensionsBrowserProxyImpl.setInstance(browserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    page = document.createElement('safety-check-extensions');
    document.body.appendChild(page);
    flush();
  });

  test('extensionsReviewUiTest', async function() {
    // Make sure that clicking the Safety Check Review button navigates
    // the user to the extensions page.
    const safetyCheck = page.$.safetyCheckChild.shadowRoot!;
    assertTrue(!!safetyCheck);
    safetyCheck.querySelector<HTMLElement>('#button')!.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('chrome://extensions', url);
  });
});


suite('SafetyCheckExtensionsChildUiTests', function() {
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let page: SettingsSafetyCheckExtensionsChildElement;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-check-extensions-child');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  /** @return {!Promise} */
  async function expectExtensionsButtonClickActions() {
    // User clicks review extensions button.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>('#safetyCheckChild')!
        .shadowRoot!.querySelector<HTMLElement>('#button')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.EXTENSIONS_REVIEW,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ReviewExtensions',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    assertEquals(
        'chrome://extensions', await openWindowProxy.whenCalled('openUrl'));
  }

  test('extensionsCheckingUiTest', function() {
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.CHECKING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Extensions',
      sublabel: testDisplayString,
    });
  });

  test('extensionsErrorUiTest', function() {
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.ERROR);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Extensions',
      sublabel: testDisplayString,
      rowClickable: true,
    });
  });

  test('extensionsSafeUiTest', async function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Extensions',
      sublabel: testDisplayString,
      rowClickable: true,
    });

    // User clicks the row.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>(
            '#safetyCheckChild')!.click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.EXTENSIONS_CARET_NAVIGATION,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ReviewExtensionsThroughCaretNavigation',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('chrome://extensions', url);
  });

  test('extensionsBlocklistedOffUiTest', async function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Extensions',
      sublabel: testDisplayString,
      rowClickable: true,
    });

    // User clicks the row.
    page.shadowRoot!
        .querySelector<SettingsSafetyCheckChildElement>(
            '#safetyCheckChild')!.click();
    // Ensure the browser proxy call is done.
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('chrome://extensions', url);
  });

  test('extensionsBlocklistedOnAllUserUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Extensions',
      sublabel: testDisplayString,
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review extensions',
      buttonClass: 'action-button',
    });
    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnUserAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Extensions',
      sublabel: testDisplayString,
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review extensions',
      buttonClass: 'action-button',
      managedIcon: false,
    });
    return expectExtensionsButtonClickActions();
  });

  test('extensionsBlocklistedOnAllAdminUiTest', function() {
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Extensions',
      sublabel: testDisplayString,
      managedIcon: true,
      rowClickable: true,
    });
  });
});
