// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyGuideCompletionFragmentElement, PrivacyGuideHistorySyncFragmentElement, PrivacyGuideStep, PrivacyGuideWelcomeFragmentElement, SafeBrowsingSetting, SettingsPrivacyGuideDialogElement, SettingsPrivacyGuidePageElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction, CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideSettingsStates, Router, routes, SettingsPrefsElement, StatusAction, SyncBrowserProxyImpl, SyncPrefs, syncPrefsIndividualDataTypes, SyncStatus} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {getSyncAllPrefs} from './sync_test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';

// clang-format on

/* Maximum number of steps in the privacy guide, excluding the welcome and
 * completion steps.
 */
const PRIVACY_GUIDE_STEPS = 4;

const SETTINGS_FRAGMENT_NAMES = [
  'privacy-guide-msbb-fragment',
  'privacy-guide-history-sync-fragment',
  'privacy-guide-safe-browsing-fragment',
  'privacy-guide-cookies-fragment',
  'privacy-guide-clear-on-exit-fragment',
];

function setupPrivacyRouteForTest() {
  // Simulates the route of the user entering the privacy guide from the S&P
  // settings. This is necessary as tests seem to by default define the
  // previous route as Settings "/". On a back navigation, "/" matches the
  // criteria for a valid Settings parent no matter how deep the subpage is in
  // the Settings tree. This would always navigate to Settings "/" instead of
  // to the parent of the current subpage.
  Router.getInstance().navigateTo(routes.PRIVACY);
}

/**
 * Equivalent of the user manually navigating to the corresponding step via
 * typing the URL and step parameter in the Omnibox.
 */
function navigateToStep(step: PrivacyGuideStep) {
  Router.getInstance().navigateTo(
      routes.PRIVACY_GUIDE,
      /* opt_dynamicParameters */ new URLSearchParams('step=' + step));
  return flushTasks();
}

/**
 * Fire a sign in status change event and flush the UI.
 */
function setSignInState(signedIn: boolean) {
  const event = {
    signedIn: signedIn,
  };
  webUIListenerCallback('update-sync-state', event);
  flush();
}

/**
 * Set all relevant sync status and fire a changed event and flush the UI.
 */
function setupSync({
  syncBrowserProxy,
  syncOn,
  syncAllDataTypes,
  typedUrlsSynced,
}: {
  syncBrowserProxy: TestSyncBrowserProxy,
  syncAllDataTypes: boolean,
  typedUrlsSynced: boolean,
  syncOn: boolean,
}) {
  if (syncAllDataTypes) {
    assertTrue(typedUrlsSynced);
  }
  if (typedUrlsSynced) {
    assertTrue(syncOn);
  }
  syncBrowserProxy.testSyncStatus = {
    signedIn: syncOn,
    hasError: false,
    statusAction: StatusAction.NO_ACTION,
  };
  webUIListenerCallback('sync-status-changed', syncBrowserProxy.testSyncStatus);

  const event = getSyncAllPrefs();
  // Overwrite datatypes needed in tests.
  event.syncAllDataTypes = syncAllDataTypes;
  event.typedUrlsSynced = typedUrlsSynced;
  webUIListenerCallback('sync-prefs-changed', event);
}

/**
 * Returns a new promise that resolves after a window 'popstate' event.
 */
function whenPopState(causeEvent: () => void): Promise<void> {
  const promise = eventToPromise('popstate', window);
  causeEvent();
  return promise;
}

// Set the cookies setting for the privacy guide.
function setCookieSetting(
    page: SettingsPrivacyGuidePageElement, setting: CookiePrimarySetting) {
  page.set('prefs.generated.cookie_primary_setting', {
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: setting,
  });
}

function shouldShowCookiesCard(page: SettingsPrivacyGuidePageElement): boolean {
  const setting = page.getPref('generated.cookie_primary_setting').value;
  return setting === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
      setting === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
}

// Set the safe browsing setting for the privacy guide.
function setSafeBrowsingSetting(
    page: SettingsPrivacyGuidePageElement, setting: SafeBrowsingSetting) {
  page.set('prefs.generated.safe_browsing', {
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: setting,
  });
}

function shouldShowSafeBrowsingCard(page: SettingsPrivacyGuidePageElement):
    boolean {
  const setting = page.getPref('generated.safe_browsing').value;
  return setting === SafeBrowsingSetting.ENHANCED ||
      setting === SafeBrowsingSetting.STANDARD;
}

function assertQueryParameter(step: PrivacyGuideStep) {
  assertEquals(step, Router.getInstance().getQueryParameters().get('step'));
}

function shouldShowHistorySyncCard(syncBrowserProxy: TestSyncBrowserProxy):
    boolean {
  return !syncBrowserProxy.testSyncStatus ||
      !!syncBrowserProxy.testSyncStatus.signedIn;
}

interface AssertCardComponentsVisibleParams {
  page: SettingsPrivacyGuidePageElement;
  isSettingFooterVisibleExpected?: boolean;
  isBackButtonVisibleExpected?: boolean;
  isWelcomeFragmentVisibleExpected?: boolean;
  isCompletionFragmentVisibleExpected?: boolean;
  isMsbbFragmentVisibleExpected?: boolean;
  isClearOnExitFragmentVisibleExpected?: boolean;
  isHistorySyncFragmentVisibleExpected?: boolean;
  isSafeBrowsingFragmentVisibleExpected?: boolean;
  isCookiesFragmentVisibleExpected?: boolean;
}

function assertCardComponentsVisible({
  page,
  isSettingFooterVisibleExpected,
  isBackButtonVisibleExpected,
  isWelcomeFragmentVisibleExpected,
  isCompletionFragmentVisibleExpected,
  isMsbbFragmentVisibleExpected,
  isClearOnExitFragmentVisibleExpected,
  isHistorySyncFragmentVisibleExpected,
  isSafeBrowsingFragmentVisibleExpected,
  isCookiesFragmentVisibleExpected,
}: AssertCardComponentsVisibleParams) {
  assertEquals(
      !!isSettingFooterVisibleExpected, isChildVisible(page, '#settingFooter'));
  if (isSettingFooterVisibleExpected) {
    const backButtonVisibility =
        getComputedStyle(
            page.shadowRoot!.querySelector<HTMLElement>('#backButton')!)
            .visibility;
    assertEquals(
        isBackButtonVisibleExpected ? 'visible' : 'hidden',
        backButtonVisibility);
  }
  assertEquals(
      !!isWelcomeFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.WELCOME));
  assertEquals(
      !!isCompletionFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.COMPLETION));
  assertEquals(
      !!isMsbbFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.MSBB));
  assertEquals(
      !!isClearOnExitFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.CLEAR_ON_EXIT));
  assertEquals(
      !!isHistorySyncFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.HISTORY_SYNC));
  assertEquals(
      !!isSafeBrowsingFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.SAFE_BROWSING));
  assertEquals(
      !!isCookiesFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.COOKIES));
}

/**
 * @return The expected total number of active cards for the step indicator.
 */
function getExpectedNumberOfActiveCards(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  let numSteps = PRIVACY_GUIDE_STEPS;
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    numSteps -= 1;
  }
  if (!shouldShowCookiesCard(page)) {
    numSteps -= 1;
  }
  if (!shouldShowSafeBrowsingCard(page)) {
    numSteps -= 1;
  }
  return numSteps;
}

function assertStepIndicatorModel(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy, activeIndex: number) {
  const model = page.computeStepIndicatorModel();
  assertEquals(activeIndex, model.active);
  assertEquals(
      getExpectedNumberOfActiveCards(page, syncBrowserProxy), model.total);
}

function assertWelcomeCardVisible(page: SettingsPrivacyGuidePageElement) {
  assertQueryParameter(PrivacyGuideStep.WELCOME);
  assertCardComponentsVisible({
    page: page,
    isWelcomeFragmentVisibleExpected: true,
  });
}

function assertCompletionCardVisible(page: SettingsPrivacyGuidePageElement) {
  assertQueryParameter(PrivacyGuideStep.COMPLETION);
  assertCardComponentsVisible({
    page: page,
    isCompletionFragmentVisibleExpected: true,
  });
}

function assertMsbbCardVisible(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  assertQueryParameter(PrivacyGuideStep.MSBB);
  assertCardComponentsVisible({
    page: page,
    isSettingFooterVisibleExpected: true,
    isBackButtonVisibleExpected: true,
    isMsbbFragmentVisibleExpected: true,
  });
  assertStepIndicatorModel(page, syncBrowserProxy, 0);
}

function assertHistorySyncCardVisible(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  assertQueryParameter(PrivacyGuideStep.HISTORY_SYNC);
  assertCardComponentsVisible({
    page: page,
    isSettingFooterVisibleExpected: true,
    isBackButtonVisibleExpected: true,
    isHistorySyncFragmentVisibleExpected: true,
  });
  assertStepIndicatorModel(page, syncBrowserProxy, 1);
}

function assertSafeBrowsingCardVisible(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  assertQueryParameter(PrivacyGuideStep.SAFE_BROWSING);
  assertCardComponentsVisible({
    page: page,
    isSettingFooterVisibleExpected: true,
    isBackButtonVisibleExpected: true,
    isSafeBrowsingFragmentVisibleExpected: true,
  });
  assertStepIndicatorModel(
      page, syncBrowserProxy,
      shouldShowHistorySyncCard(syncBrowserProxy) ? 2 : 1);
}

function assertCookiesCardVisible(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  assertQueryParameter(PrivacyGuideStep.COOKIES);
  assertCardComponentsVisible({
    page: page,
    isSettingFooterVisibleExpected: true,
    isBackButtonVisibleExpected: true,
    isCookiesFragmentVisibleExpected: true,
  });
  let activeIndex = 3;
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    activeIndex -= 1;
  }
  if (!shouldShowSafeBrowsingCard(page)) {
    activeIndex -= 1;
  }
  assertStepIndicatorModel(page, syncBrowserProxy, activeIndex);
}

// Bundles functionality to create the page object for tests.
function createPrivacyGuidePageForTest(settingsPrefs: SettingsPrefsElement) {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const page = document.createElement('settings-privacy-guide-page');
  page.disableAnimationsForTesting();
  page.prefs = settingsPrefs.prefs!;
  document.body.appendChild(page);

  setupPrivacyRouteForTest();

  return page;
}

// Bundles frequently used functionality to configure the page object for tests.
function setupPrivacyGuidePageForTest(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  setSafeBrowsingSetting(page, SafeBrowsingSetting.STANDARD);
  setCookieSetting(page, CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);
  setupSync({
    syncBrowserProxy: syncBrowserProxy,
    syncOn: true,
    syncAllDataTypes: true,
    typedUrlsSynced: true,
  });
}

suite('PrivacyGuidePageTests', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('welcomeForwardNavigation', async function() {
    assertFalse(page.getPref('privacy_guide.viewed').value);

    // Navigating to the privacy guide without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();
    assertWelcomeCardVisible(page);

    assertTrue(page.getPref('privacy_guide.viewed').value);

    const welcomeFragment =
        page.shadowRoot!.querySelector<PrivacyGuideWelcomeFragmentElement>(
            '#' + PrivacyGuideStep.WELCOME)!;
    welcomeFragment.$.startButton.click();
    flush();
    assertMsbbCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.WELCOME_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickWelcome');

    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    assertMsbbCardVisible(page, syncBrowserProxy);
  });

  test('completionCardBackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.COMPLETION);
    assertCompletionCardVisible(page);

    const completionFragment =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.COMPLETION)!;
    completionFragment.shadowRoot!.querySelector<HTMLElement>(
                                      '#backButton')!.click();
    flush();
    assertCookiesCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCompletion');
  });

  test('privacyGuideVisibilityChildAccount', async function() {
    // Set the user to have a non-child account.
    const syncStatus:
        SyncStatus = {childUser: false, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);

    // Navigating to the privacy guide works.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();
    assertWelcomeCardVisible(page);

    // The user signs in to a child user account. This hides the privacy guide
    // and navigates away back to privacy settings page.
    const newSyncStatus:
        SyncStatus = {childUser: true, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', newSyncStatus);
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());

    // User trying to manually navigate to privacy guide fails.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('privacyGuideVisibilityManagedAccount', async function() {
    // Set the user to have a non-managed account.
    webUIListenerCallback('is-managed-changed', false);

    // Navigating to the privacy guide works.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();
    assertWelcomeCardVisible(page);

    // The user signs in to a managed account. This hides the privacy guide and
    // navigates away back to privacy settings page.
    webUIListenerCallback('is-managed-changed', true);
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());

    // User trying to manually navigate to privacy guide fails.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('arrowKeyNavigation', async function() {
    const pgCard =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyGuideCard')!;
    const arrowLeftEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});
    const arrowRightEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});
    function dispatchArrowLeftEvent() {
      pgCard.dispatchEvent(arrowLeftEvent);
      flush();
    }
    function dispatchArrowRightEvent() {
      pgCard.dispatchEvent(arrowRightEvent);
      flush();
    }

    // Ensure a defined text direction.
    loadTimeData.overrideValues({textdirection: 'ltr'});

    // Forward flow.
    await navigateToStep(PrivacyGuideStep.WELCOME);
    dispatchArrowRightEvent();
    assertMsbbCardVisible(page, syncBrowserProxy);
    dispatchArrowRightEvent();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
    dispatchArrowRightEvent();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    // Arrow keys don't trigger a navigation when the focus is inside the radio
    // group.
    const sbRadioGroup =
        page.shadowRoot!
            .querySelector<HTMLElement>('#' + PrivacyGuideStep.SAFE_BROWSING)!
            .shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup')!;
    sbRadioGroup.dispatchEvent(arrowLeftEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    sbRadioGroup.dispatchEvent(arrowRightEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    dispatchArrowRightEvent();
    assertCookiesCardVisible(page, syncBrowserProxy);
    // Arrow keys don't trigger a navigation when the focus is inside the radio
    // group.
    const cookiesRadioGroup =
        page.shadowRoot!
            .querySelector<HTMLElement>('#' + PrivacyGuideStep.COOKIES)!
            .shadowRoot!.querySelector<HTMLElement>('#cookiesRadioGroup')!;
    cookiesRadioGroup.dispatchEvent(arrowLeftEvent);
    assertCookiesCardVisible(page, syncBrowserProxy);
    cookiesRadioGroup.dispatchEvent(arrowRightEvent);
    assertCookiesCardVisible(page, syncBrowserProxy);

    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);
    // Forward navigation on the completion card does not trigger a navigation.
    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);

    // Backward flow.
    dispatchArrowLeftEvent();
    assertCookiesCardVisible(page, syncBrowserProxy);
    dispatchArrowLeftEvent();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    dispatchArrowLeftEvent();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
    dispatchArrowLeftEvent();
    assertMsbbCardVisible(page, syncBrowserProxy);
    dispatchArrowLeftEvent();
    assertWelcomeCardVisible(page);
    // Backward navigation on the welcome card does not trigger a navigation.
    dispatchArrowLeftEvent();
    assertWelcomeCardVisible(page);
  });
});

suite('MsbbFragmentNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('msbbBackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertWelcomeCardVisible(page);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickMSBB');
  });

  test('msbbForwardNavigationSyncOn', async function() {
    await navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.MSBB_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickMSBB');
  });

  test('msbbForwardNavigationSyncOff', async function() {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    await navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });
});

suite('HistorySyncFragmentNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('historySyncBackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickHistorySync');
  });

  test('historySyncNavigatesAwayOnSyncOff', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    // User disables sync while history sync card is shown.
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });

  test('historySyncNotReachableWhenSyncOff', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });

  test(
      'historySyncCardForwardNavigationShouldShowSafeBrowsingCard',
      async function() {
        await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
        assertHistorySyncCardVisible(page, syncBrowserProxy);

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertSafeBrowsingCardVisible(page, syncBrowserProxy);

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideNextNavigationHistogram');
        assertEquals(PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON, result);

        const actionResult =
            await testMetricsBrowserProxy.whenCalled('recordAction');
        assertEquals(
            actionResult, 'Settings.PrivacyGuide.NextClickHistorySync');
      });

  test(
      'historySyncCardForwardNavigationShouldHideSafeBrowsingCard',
      async function() {
        setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
        await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
        assertHistorySyncCardVisible(page, syncBrowserProxy);

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertCookiesCardVisible(page, syncBrowserProxy);
      });
});

suite('SafeBrowsingFragmentNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('safeBrowsingCardBackNavigationSyncOn', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickSafeBrowsing');
  });

  test('safeBrowsingCardBackNavigationSyncOff', async function() {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible(page, syncBrowserProxy);
  });

  test('safeBrowsingCardGetsUpdated', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    const radioButtonGroup =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.SAFE_BROWSING)!
            .shadowRoot!.querySelector<SettingsRadioGroupElement>(
                '#safeBrowsingRadioGroup')!;
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.STANDARD);

    // Changing the safe browsing setting should automatically change the
    // selected radio button.
    setSafeBrowsingSetting(page, SafeBrowsingSetting.ENHANCED);
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.ENHANCED);

    // Changing the safe browsing setting to a disabled state while shown should
    // navigate away from the safe browsing card.
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    assertCookiesCardVisible(page, syncBrowserProxy);
  });

  test(
      'safeBrowsingCardForwardNavigationShouldShowCookiesCard',
      async function() {
        await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
        assertSafeBrowsingCardVisible(page, syncBrowserProxy);

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        flush();
        assertCookiesCardVisible(page, syncBrowserProxy);

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideNextNavigationHistogram');
        assertEquals(
            PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON, result);

        const actionResult =
            await testMetricsBrowserProxy.whenCalled('recordAction');
        assertEquals(
            actionResult, 'Settings.PrivacyGuide.NextClickSafeBrowsing');
      });

  test(
      'safeBrowsingCardForwardNavigationShouldHideCookiesCard',
      async function() {
        setCookieSetting(page, CookiePrimarySetting.ALLOW_ALL);
        await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
        assertSafeBrowsingCardVisible(page, syncBrowserProxy);

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        flush();
        assertCompletionCardVisible(page);
      });
});

suite('CookiesFragmentNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('cookiesCardBackNavigationShouldShowSafeBrowsingCard', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCookies');
  });

  test('cookiesCardBackNavigationShouldHideSafeBrowsingCard', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
  });

  test('cookiesCardForwardNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COOKIES_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCookies');
  });

  test('cookiesCardGetsUpdated', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);
    const radioButtonGroup =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.COOKIES)!
            .shadowRoot!.querySelector<SettingsRadioGroupElement>(
                '#cookiesRadioGroup')!;
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);

    // Changing the cookie setting should automatically change the selected
    // radio button.
    setCookieSetting(page, CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(page, CookiePrimarySetting.ALLOW_ALL);
    await flushTasks();
    assertCompletionCardVisible(page);
  });

  test('hatsInformedOnFinish', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();

    // HaTS gets triggered if the user navigates to the completion page.
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE, interaction);
  });
});

suite('MsbbFragmentMetricsTests', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  async function assertMsbbMetrics({
    msbbStartOn,
    changeSetting,
    expectedMetric,
  }: {
    msbbStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    page.setPrefValue(
        'url_keyed_anonymized_data_collection.enabled', msbbStartOn);
    await navigateToStep(PrivacyGuideStep.MSBB);

    if (changeSetting) {
      page.shadowRoot!.querySelector('#' + PrivacyGuideStep.MSBB)!.shadowRoot!
          .querySelector<HTMLElement>('#urlCollectionToggle')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          msbbStartOn ? 'Settings.PrivacyGuide.ChangeMSBBOff' :
                        'Settings.PrivacyGuide.ChangeMSBBOn');
    }

    // Go back instead of forward to not need sync state in the test.
    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('msbbMetricsOnToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_ON,
    });
  });

  test('msbbMetricsOnToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_OFF,
    });
  });

  test('msbbMetricsOffToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_ON,
    });
  });

  test('msbbMetricsOffToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF,
    });
  });
});

suite('HistorySyncFragmentMetricsTests', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  async function assertHistorySyncMetrics({
    historySyncStartOn,
    changeSetting,
    expectedMetric,
  }: {
    historySyncStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: historySyncStartOn,
      typedUrlsSynced: historySyncStartOn,
    });
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);

    if (changeSetting) {
      page.shadowRoot!.querySelector('#' + PrivacyGuideStep.HISTORY_SYNC)!
          .shadowRoot!.querySelector<HTMLElement>('#historyToggle')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          historySyncStartOn ? 'Settings.PrivacyGuide.ChangeHistorySyncOff' :
                               'Settings.PrivacyGuide.ChangeHistorySyncOn');
    }

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('historySyncOnToOn', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON,
    });
  });

  test('historySyncOnToOff', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF,
    });
  });

  test('historySyncOffToOn', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON,
    });
  });

  test('historySyncOffToOff', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF,
    });
  });
});

suite('SafeBrowsingFragmentMetricsTests', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  async function assertSafeBrowsingMetrics({
    safeBrowsingStartsEnhanced,
    changeSetting,
    expectedMetric,
  }: {
    safeBrowsingStartsEnhanced: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const safeBrowsingStartState = safeBrowsingStartsEnhanced ?
        SafeBrowsingSetting.ENHANCED :
        SafeBrowsingSetting.STANDARD;
    page.setPrefValue('generated.safe_browsing', safeBrowsingStartState);
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);

    if (changeSetting) {
      page.shadowRoot!.querySelector(
                          '#' + PrivacyGuideStep.SAFE_BROWSING)!.shadowRoot!
          .querySelector<HTMLElement>(
              safeBrowsingStartsEnhanced ?
                  '#safeBrowsingRadioStandard' :
                  '#safeBrowsingRadioEnhanced')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          safeBrowsingStartsEnhanced ?
              'Settings.PrivacyGuide.ChangeSafeBrowsingStandard' :
              'Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced');
    }

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('safeBrowsingMetricsEnhancedToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsStandardToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED,
    });
  });

  test('safeBrowsingMetricsStandardToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsEnhancedToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED,
    });
  });
});

suite('CookiesFragmentMetricsTests', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  async function assertCookieMetrics({
    cookieStartsBlock3PIncognito,
    changeSetting,
    expectedMetric,
  }: {
    cookieStartsBlock3PIncognito: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const cookieStartState = cookieStartsBlock3PIncognito ?
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO :
        CookiePrimarySetting.BLOCK_THIRD_PARTY;
    page.setPrefValue('generated.cookie_primary_setting', cookieStartState);
    await navigateToStep(PrivacyGuideStep.COOKIES);

    if (changeSetting) {
      page.shadowRoot!.querySelector(
                          '#' + PrivacyGuideStep.COOKIES)!.shadowRoot!
          .querySelector<HTMLElement>(
              cookieStartsBlock3PIncognito ? '#block3P' :
                                             '#block3PIncognito')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          cookieStartsBlock3PIncognito ?
              'Settings.PrivacyGuide.ChangeCookiesBlock3P' :
              'Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito');
    }

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('cookiesMetrics3PIncognitoTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PIncognitoTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P,
    });
  });

  test('cookiesMetrics3PTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P,
    });
  });
});

suite('HistorySyncFragment', function() {
  let page: PrivacyGuideHistorySyncFragmentElement;
  let syncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-guide-history-sync-fragment');
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  function setSyncStatus({
    syncAllDataTypes,
    typedUrlsSynced,
    passwordsSynced,
  }: {
    syncAllDataTypes: boolean,
    typedUrlsSynced: boolean,
    passwordsSynced: boolean,
  }) {
    if (syncAllDataTypes) {
      assertTrue(typedUrlsSynced);
      assertTrue(passwordsSynced);
    }
    const event: SyncPrefs = {} as unknown as SyncPrefs;
    for (const datatype of syncPrefsIndividualDataTypes) {
      (event as unknown as {[key: string]: boolean})[datatype] = true;
    }
    // Overwrite datatypes needed in tests.
    event.syncAllDataTypes = syncAllDataTypes;
    event.typedUrlsSynced = typedUrlsSynced;
    event.passwordsSynced = passwordsSynced;
    webUIListenerCallback('sync-prefs-changed', event);
  }

  async function assertBrowserProxyCall({
    syncAllDatatypesExpected,
    typedUrlsSyncedExpected,
  }: {
    syncAllDatatypesExpected: boolean,
    typedUrlsSyncedExpected: boolean,
  }) {
    const syncPrefs = await syncBrowserProxy.whenCalled('setSyncDatatypes');
    assertEquals(syncAllDatatypesExpected, syncPrefs.syncAllDataTypes);
    assertEquals(typedUrlsSyncedExpected, syncPrefs.typedUrlsSynced);
    syncBrowserProxy.resetResolver('setSyncDatatypes');
  }

  test('syncAllOnDisableReenableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync re-enables sync all if sync all was on before
    // and if all sync datatypes are still enabled.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: true,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncOtherDatatypeOff', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // The user disables another datatype in a different tab.
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: false,
    });

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncWithNavigation', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // The user navigates to another card, then back to the history sync card.
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=msbb'));
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=historySync'));

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffDisableReenableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync doesn't re-enable sync all if sync all wasn't on
    // originally.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffEnableHistorySync', function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });
});

suite('CompletionFragment', function() {
  let page: PrivacyGuideCompletionFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(page);

    setupPrivacyRouteForTest();
    // The user navigates to the completion step.
    return navigateToStep(PrivacyGuideStep.COMPLETION);
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('backToSettingsNavigation', async function() {
    const closeEventPromise = eventToPromise('close', page);

    page.shadowRoot!.querySelector<HTMLElement>('#leaveButton')!.click();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCompletion');

    // Ensure the |close| event has been sent.
    return closeEventPromise;
  });

  test('SWAALinkClick', async function() {
    setSignInState(true);

    assertTrue(isChildVisible(page, '#waaRow'));
    page.shadowRoot!.querySelector<HTMLElement>('#waaRow')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SWAA_COMPLETION_LINK, result);
  });

  test('privacySandboxLinkClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>('#privacySandboxRow')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(
        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK, result);
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertTrue(isChildVisible(page, '#privacySandboxRow'));
    assertTrue(isChildVisible(page, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(page, '#privacySandboxRow'));
    assertFalse(isChildVisible(page, '#waaRow'));
  });
});

suite('CompletionFragmentPrivacySandboxRestricted', function() {
  let page: PrivacyGuideCompletionFragmentElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(page);

    setupPrivacyRouteForTest();
    // The user navigates to the completion step.
    navigateToStep(PrivacyGuideStep.COMPLETION);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertFalse(isChildVisible(page, '#privacySandboxRow'));
    assertTrue(isChildVisible(page, '#waaRow'));
    const subheader =
        page.shadowRoot!.querySelector<HTMLElement>('.cr-secondary-text')!;
    assertEquals(
        page.i18n('privacyGuideCompletionCardSubHeader'), subheader.innerText);

    setSignInState(false);
    assertFalse(isChildVisible(page, '#privacySandboxRow'));
    assertFalse(isChildVisible(page, '#waaRow'));
    assertEquals(
        page.i18n('privacyGuideCompletionCardSubHeaderNoLinks'),
        subheader.innerText);
  });
});

// TODO(1215630): Remove once #privacy-guide-2 has been rolled out.
suite('CompletionFragmentPrivacyGuide2Disabled', function() {
  let page: PrivacyGuideCompletionFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacyGuide2Enabled: false,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(page);

    setupPrivacyRouteForTest();
    // The user navigates to the completion step.
    return navigateToStep(PrivacyGuideStep.COMPLETION);
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('backToSettingsNavigation', function() {
    return whenPopState(async function() {
             page.shadowRoot!.querySelector<HTMLElement>(
                                 '#leaveButton')!.click();

             const result = await testMetricsBrowserProxy.whenCalled(
                 'recordPrivacyGuideNextNavigationHistogram');
             assertEquals(
                 PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON, result);

             const actionResult =
                 await testMetricsBrowserProxy.whenCalled('recordAction');
             assertEquals(
                 actionResult, 'Settings.PrivacyGuide.NextClickCompletion');
           })
        .then(function() {
          assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
        });
  });
});

suite('PrivacyGuideDialog', function() {
  let page: SettingsPrivacyGuideDialogElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-privacy-guide-dialog');
    document.body.appendChild(page);

    setupPrivacyRouteForTest();

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);

    // The user navigates to PG.
    return navigateToStep(PrivacyGuideStep.WELCOME);
  });

  test('closeEventClosesDialog', function() {
    assertTrue(page.$.dialog.open);

    // A |close| event from the embedded PG page closes the PG dialog.
    page.shadowRoot!.querySelector<HTMLElement>('settings-privacy-guide-page')!
        .dispatchEvent(
            new CustomEvent('close', {bubbles: true, composed: true}));

    assertFalse(page.$.dialog.open);
  });
});

// TODO(1215630): Remove once #privacy-guide-2 has been rolled out.
suite('CardHeaderTestsPrivacyGuide2Enabled', function() {
  test('phase2HeadersVisible', function() {
    for (const fragmentName of SETTINGS_FRAGMENT_NAMES) {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      const page = document.createElement(fragmentName);
      document.body.appendChild(page);
      flush();

      assertFalse(
          isChildVisible(page, '.header'), fragmentName + ' phase1 header');
      assertTrue(
          isChildVisible(page, '.header-phase2'),
          fragmentName + ' phase2 header');
    }
  });
});

// TODO(1215630): Remove once #privacy-guide-2 has been rolled out.
suite('CardHeaderTestsPrivacyGuide2Disabled', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacyGuide2Enabled: false,
    });
  });

  test('phase1HeadersVisible', function() {
    for (const fragmentName of SETTINGS_FRAGMENT_NAMES) {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      const page = document.createElement(fragmentName);
      document.body.appendChild(page);
      flush();

      assertTrue(
          isChildVisible(page, '.header'), fragmentName + ' phase1 header');
      assertFalse(
          isChildVisible(page, '.header-phase2'),
          fragmentName + ' phase2 header');
    }
  });
});
