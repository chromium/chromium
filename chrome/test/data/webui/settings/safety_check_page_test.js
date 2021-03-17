// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {HatsBrowserProxyImpl, LifetimeBrowserProxyImpl, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerProxy, Router, routes, SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckChromeCleanerStatus, SafetyCheckExtensionsStatus, SafetyCheckIconStatus, SafetyCheckInteractions, SafetyCheckParentStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

const testDisplayString = 'Test display string';

/**
 * Fire a safety check parent event.
 * @param {!SafetyCheckParentStatus} state
 */
function fireSafetyCheckParentEvent(state) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.PARENT_CHANGED, event);
}

/**
 * Fire a safety check updates event.
 * @param {!SafetyCheckUpdatesStatus} state
 */
function fireSafetyCheckUpdatesEvent(state) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.UPDATES_CHANGED, event);
}

/**
 * Fire a safety check passwords event.
 * @param {!SafetyCheckPasswordsStatus} state
 */
function fireSafetyCheckPasswordsEvent(state) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  event.passwordsButtonString = null;
  webUIListenerCallback(SafetyCheckCallbackConstants.PASSWORDS_CHANGED, event);
}

/**
 * Fire a safety check safe browsing event.
 * @param {!SafetyCheckSafeBrowsingStatus} state
 */
function fireSafetyCheckSafeBrowsingEvent(state) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(
      SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED, event);
}

/**
 * Fire a safety check extensions event.
 * @param {!SafetyCheckExtensionsStatus} state
 */
function fireSafetyCheckExtensionsEvent(state) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(SafetyCheckCallbackConstants.EXTENSIONS_CHANGED, event);
}

/**
 * Fire a safety check Chrome cleaner event.
 * @param {SafetyCheckChromeCleanerStatus} state
 */
function fireSafetyCheckChromeCleanerEvent(state) {
  const event = {
    newState: state,
    displayString: testDisplayString,
  };
  webUIListenerCallback(
      SafetyCheckCallbackConstants.CHROME_CLEANER_CHANGED, event);
}

/**
 * Verify that the safety check child inside the page has been configured as
 * specified.
 * @param {!{
 *   page: !PolymerElement,
 *   iconStatus: !SafetyCheckIconStatus,
 *   label: string,
 *   buttonLabel: (string|undefined),
 *   buttonAriaLabel: (string|undefined),
 *   buttonClass: (string|undefined),
 *   managedIcon: (boolean|undefined),
 *   rowClickable: (boolean|undefined),
 * }} destructured1
 */
function assertSafetyCheckChild({
  page,
  iconStatus,
  label,
  buttonLabel,
  buttonAriaLabel,
  buttonClass,
  managedIcon,
  rowClickable
}) {
  const safetyCheckChild = page.$$('#safetyCheckChild');
  assertTrue(safetyCheckChild.iconStatus === iconStatus);
  assertTrue(safetyCheckChild.label === label);
  assertTrue(safetyCheckChild.subLabel === testDisplayString);
  assertTrue(!buttonLabel || safetyCheckChild.buttonLabel === buttonLabel);
  assertTrue(
      !buttonAriaLabel || safetyCheckChild.buttonAriaLabel === buttonAriaLabel);
  assertTrue(!buttonClass || safetyCheckChild.buttonClass === buttonClass);
  assertTrue(!!managedIcon === !!safetyCheckChild.managedIcon);
  assertTrue(!!rowClickable === !!safetyCheckChild.rowClickable);
}

/** @implements {SafetyCheckBrowserProxy} */
class TestSafetyCheckBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getParentRanDisplayString',
      'runSafetyCheck',
    ]);

    /** @private {string} */
    this.parentRanDisplayString_ = '';
  }

  /** @override */
  runSafetyCheck() {
    this.methodCalled('runSafetyCheck');
  }

  /** @param {string} string */
  setParentRanDisplayString(string) {
    this.parentRanDisplayString_ = string;
  }

  /** @override */
  getParentRanDisplayString() {
    this.methodCalled('getParentRanDisplayString');
    return Promise.resolve(this.parentRanDisplayString_);
  }
}

suite('SafetyCheckPageUiTests', function() {
  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {?TestSafetyCheckBrowserProxy} */
  let safetyCheckBrowserProxy = null;

  /** @type {!SettingsSafetyCheckPageElement} */
  let page;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;

    safetyCheckBrowserProxy = new TestSafetyCheckBrowserProxy();
    safetyCheckBrowserProxy.setParentRanDisplayString('Dummy string');
    SafetyCheckBrowserProxyImpl.instance_ = safetyCheckBrowserProxy;

    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckPageElement} */ (
        document.createElement('settings-safety-check-page'));
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  /** Tests parent element and collapse.from start to completion */
  test('testParentAndCollapse', async function() {
    // Before the check, only the text button is present.
    assertTrue(!!page.$$('#safetyCheckParentButton'));
    assertFalse(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is not opened.
    const collapse =
        /** @type {!IronCollapseElement} */ (page.$$('#safetyCheckCollapse'));
    assertFalse(collapse.opened);

    // User starts check.
    page.$$('#safetyCheckParentButton').click();
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
    fireSafetyCheckChromeCleanerEvent(SafetyCheckChromeCleanerStatus.CHECKING);
    fireSafetyCheckParentEvent(SafetyCheckParentStatus.CHECKING);

    flush();
    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(collapse.opened);

    // Mock all incoming messages that indicate safety check completion.
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATED);
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.SAFE);
    fireSafetyCheckSafeBrowsingEvent(
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD);
    fireSafetyCheckExtensionsEvent(
        SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS);
    fireSafetyCheckChromeCleanerEvent(SafetyCheckChromeCleanerStatus.INFECTED);
    fireSafetyCheckParentEvent(SafetyCheckParentStatus.AFTER);

    flush();
    // Only the icon button is present.
    assertFalse(!!page.$$('#safetyCheckParentButton'));
    assertTrue(!!page.$$('#safetyCheckParentIconButton'));
    // Collapse is opened.
    assertTrue(page.$$('#safetyCheckCollapse').opened);

    // Ensure the automatic browser proxy calls are started.
    return safetyCheckBrowserProxy.whenCalled('getParentRanDisplayString');
  });

  test('HappinessTrackingSurveysTest', function() {
    const testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.instance_ = testHatsBrowserProxy;
    page.$$('#safetyCheckParentButton').click();
    return testHatsBrowserProxy.whenCalled('tryShowSurvey');
  });
});

suite('SafetyCheckChildTests', function() {
  /** @type {!SettingsSafetyCheckChildElement} */
  let page;

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckChildElement} */ (
        document.createElement('settings-safety-check-child'));
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('testIconStatusRunning', function() {
    page.iconStatus = SafetyCheckIconStatus.RUNNING;
    flush();
    const statusIconElem = page.$$('#statusIcon');
    assertTrue(!!statusIconElem);
    assertTrue(statusIconElem.classList.contains('icon-blue'));
    assertFalse(statusIconElem.classList.contains('icon-red'));
    assertEquals('Running', statusIconElem.getAttribute('aria-label'));
  });

  test('testIconStatusSafe', function() {
    page.iconStatus = SafetyCheckIconStatus.SAFE;
    flush();
    const statusIconElem = page.$$('#statusIcon');
    assertTrue(!!statusIconElem);
    assertTrue(statusIconElem.classList.contains('icon-blue'));
    assertFalse(statusIconElem.classList.contains('icon-red'));
    assertEquals('Passed', statusIconElem.getAttribute('aria-label'));
  });

  test('testIconStatusInfo', function() {
    page.iconStatus = SafetyCheckIconStatus.INFO;
    flush();
    const statusIconElem = page.$$('#statusIcon');
    assertTrue(!!statusIconElem);
    assertFalse(statusIconElem.classList.contains('icon-blue'));
    assertFalse(statusIconElem.classList.contains('icon-red'));
    assertEquals('Info', statusIconElem.getAttribute('aria-label'));
  });

  test('testIconStatusWarning', function() {
    page.iconStatus = SafetyCheckIconStatus.WARNING;
    flush();
    const statusIconElem = page.$$('#statusIcon');
    assertTrue(!!statusIconElem);
    assertFalse(statusIconElem.classList.contains('icon-blue'));
    assertTrue(statusIconElem.classList.contains('icon-red'));
    assertEquals('Warning', statusIconElem.getAttribute('aria-label'));
  });

  test('testLabelText', function() {
    page.label = 'Main label test text';
    flush();
    const label = page.$$('#label');
    assertTrue(!!label);
    assertEquals('Main label test text', label.textContent.trim());
  });

  test('testSubLabelText', function() {
    page.subLabel = 'Sub label test text';
    flush();
    const subLabel = page.$$('#subLabel');
    assertTrue(!!subLabel);
    assertEquals('Sub label test text', subLabel.textContent.trim());
  });

  test('testSubLabelNoText', function() {
    // sublabel not set -> empty sublabel in element
    const subLabel = page.$$('#subLabel');
    assertTrue(!!subLabel);
    assertEquals('', subLabel.textContent.trim());
  });

  test('testButtonWithoutClass', function() {
    page.buttonLabel = 'Button label';
    page.buttonAriaLabel = 'Aria label';
    flush();
    const button = page.$$('#button');
    assertTrue(!!button);
    assertEquals('Button label', button.textContent.trim());
    assertEquals('Aria label', button.getAttribute('aria-label'));
    assertFalse(button.classList.contains('action-button'));
  });

  test('testButtonWithClass', function() {
    page.buttonLabel = 'Button label';
    page.buttonAriaLabel = 'Aria label';
    page.buttonClass = 'action-button';
    flush();
    const button = page.$$('#button');
    assertTrue(!!button);
    assertEquals('Button label', button.textContent.trim());
    assertEquals('Aria label', button.getAttribute('aria-label'));
    assertTrue(button.classList.contains('action-button'));
  });

  test('testNoButton', function() {
    // Button label not set -> no button.
    assertFalse(!!page.$$('#button'));
  });

  test('testManagedIcon', function() {
    page.managedIcon = 'cr20:domain';
    flush();
    assertTrue(!!page.$$('#managedIcon'));
  });

  test('testNoManagedIcon', function() {
    // Managed icon not set -> no managed icon.
    assertFalse(!!page.$$('#managedIcon'));
  });

  test('testRowClickableIndicator', function() {
    page.rowClickable = true;
    flush();
    assertTrue(!!page.$$('#rowClickableIndicator'));
    assertEquals(
        'cr:arrow-right',
        page.$$('#rowClickableIndicator').getAttribute('iron-icon'));
  });

  test('testExternalRowClickableIndicator', function() {
    page.rowClickable = true;
    page.external = true;
    flush();
    assertTrue(!!page.$$('#rowClickableIndicator'));
    assertEquals(
        'cr:open-in-new',
        page.$$('#rowClickableIndicator').getAttribute('iron-icon'));
  });

  test('testNoRowClickableIndicator', function() {
    // rowClickable not set -> no RowClickableIndicator.
    assertFalse(!!page.$$('#rowClickableIndicator'));
  });
});

suite('SafetyCheckUpdatesChildUiTests', function() {
  /** @type {?TestLifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;

  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {!SettingsSafetyCheckUpdatesChildElement} */
  let page;

  setup(function() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;

    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckUpdatesChildElement} */ (
        document.createElement('settings-safety-check-updates-child'));
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
    });
  });

  test('updatedUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Updates',
    });
  });

  test('updatingUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UPDATING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Updates',
    });
  });

  test('relaunchUiTest', async function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.RELAUNCH);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
      buttonLabel: page.i18n('aboutRelaunch'),
      buttonAriaLabel: page.i18n('safetyCheckUpdatesButtonAriaLabel'),
      buttonClass: 'action-button',
    });

    // User clicks the relaunch button.
    page.$$('#safetyCheckChild').$$('#button').click();
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
    });
  });

  test('failedUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.FAILED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Updates',
    });
  });

  test('unknownUiTest', function() {
    fireSafetyCheckUpdatesEvent(SafetyCheckUpdatesStatus.UNKNOWN);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Updates',
    });
  });
});

suite('SafetyCheckPasswordsChildUiTests', function() {
  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {!SettingsSafetyCheckPasswordsChildElement} */
  let page;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;

    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckPasswordsChildElement} */ (
        document.createElement('settings-safety-check-passwords-child'));
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
      label: 'Passwords',
    });
  });

  test('passwordSafeUiTest', async function() {
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.SAFE);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.SAFE,
      label: 'Passwords',
      rowClickable: true,
    });

    // User clicks the row.
    page.$$('#safetyCheckChild').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.PASSWORDS_CARET_NAVIGATION,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManagePasswordsThroughCaretNavigation',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(
        routes.CHECK_PASSWORDS, Router.getInstance().getCurrentRoute());
  });

  test('passwordCompromisedUiTest', async function() {
    fireSafetyCheckPasswordsEvent(SafetyCheckPasswordsStatus.COMPROMISED);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Passwords',
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review passwords',
      buttonClass: 'action-button',
    });

    const passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.instance_ = passwordManager;

    // User clicks the manage passwords button.
    page.$$('#safetyCheckChild').$$('#button').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.PASSWORDS_MANAGE_COMPROMISED_PASSWORDS,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManagePasswords',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(
        routes.CHECK_PASSWORDS, Router.getInstance().getCurrentRoute());

    // Ensure correct referrer sent to password check.
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK, referrer);
  });

  test('passwordWeakUiTest', async function() {
    fireSafetyCheckPasswordsEvent(
        SafetyCheckPasswordsStatus.WEAK_PASSWORDS_EXIST);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Passwords',
      rowClickable: true,
    });

    // User clicks the manage passwords button.
    page.$$('#safetyCheckChild').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.PASSWORDS_MANAGE_WEAK_PASSWORDS,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ManageWeakPasswords',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the correct Settings page is shown.
    assertEquals(
        routes.CHECK_PASSWORDS, Router.getInstance().getCurrentRoute());
  });

  test('passwordInfoStatesUiTest', function() {
    // Iterate over all states
    for (const state of Object.values(SafetyCheckPasswordsStatus)) {
      fireSafetyCheckPasswordsEvent(state);
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
            label: 'Passwords',
          });
          break;
        case SafetyCheckPasswordsStatus.QUOTA_LIMIT:
        case SafetyCheckPasswordsStatus.ERROR:
          assertSafetyCheckChild({
            page: page,
            iconStatus: SafetyCheckIconStatus.INFO,
            label: 'Passwords',
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
  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {!SettingsSafetyCheckSafeBrowsingChildElement} */
  let page;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;

    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckSafeBrowsingChildElement} */ (
        document.createElement('settings-safety-check-safe-browsing-child'));
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
      rowClickable: true,
    });

    // User clicks the row.
    page.$$('#safetyCheckChild').click();
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
      buttonLabel: 'Manage',
      buttonAriaLabel: 'Manage Safe Browsing',
      buttonClass: 'action-button',
    });

    // User clicks the manage safe browsing button.
    page.$$('#safetyCheckChild').$$('#button').click();
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
      managedIcon: true,
      rowClickable: true,
    });
  });
});

suite('SafetyCheckExtensionsChildUiTests', function() {
  /** @type {?TestMetricsBrowserProxy} */
  let metricsBrowserProxy = null;

  /** @type {?TestOpenWindowProxy} */
  let openWindowProxy = null;

  /** @type {!SettingsSafetyCheckExtensionsChildElement} */
  let page;

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = metricsBrowserProxy;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.instance_ = openWindowProxy;

    document.body.innerHTML = '';
    page = /** @type {!SettingsSafetyCheckExtensionsChildElement} */ (
        document.createElement('settings-safety-check-extensions-child'));
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  /** @return {!Promise} */
  async function expectExtensionsButtonClickActions() {
    // User clicks review extensions button.
    page.$$('#safetyCheckChild').$$('#button').click();
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
        'chrome://extensions', await openWindowProxy.whenCalled('openURL'));
  }

  test('extensionsCheckingUiTest', function() {
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.CHECKING);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.RUNNING,
      label: 'Extensions',
    });
  });

  test('extensionsErrorUiTest', function() {
    fireSafetyCheckExtensionsEvent(SafetyCheckExtensionsStatus.ERROR);
    flush();
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.INFO,
      label: 'Extensions',
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
      rowClickable: true,
    });

    // User clicks the row.
    page.$$('#safetyCheckChild').click();
    // Ensure UMA is logged.
    assertEquals(
        SafetyCheckInteractions.EXTENSIONS_CARET_NAVIGATION,
        await metricsBrowserProxy.whenCalled(
            'recordSafetyCheckInteractionHistogram'));
    assertEquals(
        'Settings.SafetyCheck.ReviewExtensionsThroughCaretNavigation',
        await metricsBrowserProxy.whenCalled('recordAction'));
    // Ensure the browser proxy call is done.
    const url = await openWindowProxy.whenCalled('openURL');
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
      rowClickable: true,
    });

    // User clicks the row.
    page.$$('#safetyCheckChild').click();
    // Ensure the browser proxy call is done.
    const url = await openWindowProxy.whenCalled('openURL');
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
      managedIcon: true,
      rowClickable: true,
    });
  });
});
