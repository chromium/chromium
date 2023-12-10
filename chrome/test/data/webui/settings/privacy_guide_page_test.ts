// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyGuideStep, SafeBrowsingSetting, SettingsPrivacyGuideDialogElement, SettingsPrivacyGuidePageElement, NetworkPredictionOptions} from 'chrome://settings/lazy_load.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction, CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyGuideInteractions, Router, routes, SettingsPrefsElement, StatusAction, SyncBrowserProxyImpl, SyncStatus} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createPrivacyGuidePageForTest, navigateToStep, clickNextOnWelcomeStep, setCookieSetting, setParametersForCookiesStep, setParametersForHistorySyncStep, setParametersForSafeBrowsingStep, setSafeBrowsingSetting, setupPrivacyGuidePageForTest, setupPrivacyRouteForTest, setupSync, shouldShowCookiesCard, shouldShowHistorySyncCard, shouldShowSafeBrowsingCard, setPreloadSetting, shouldShowPreloadCard} from './privacy_guide_test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';

// clang-format on

// Maximum number of steps in the privacy guide, excluding the welcome and
// completion steps.
// TODO(crbug.com/1215630): Remove PRIVACY_GUIDE_STEPS_PG3_OFF once
// PrivacyGuide3 launched.
const PRIVACY_GUIDE_STEPS_PG3_OFF = 4;
const PRIVACY_GUIDE_STEPS = 5;

function assertQueryParameter(step: PrivacyGuideStep) {
  assertEquals(step, Router.getInstance().getQueryParameters().get('step'));
}

interface AssertCardComponentsVisibleParams {
  page: SettingsPrivacyGuidePageElement;
  isSettingFooterVisibleExpected?: boolean;
  isBackButtonVisibleExpected?: boolean;
  isWelcomeFragmentVisibleExpected?: boolean;
  isCompletionFragmentVisibleExpected?: boolean;
  isMsbbFragmentVisibleExpected?: boolean;
  isHistorySyncFragmentVisibleExpected?: boolean;
  isSafeBrowsingFragmentVisibleExpected?: boolean;
  isCookiesFragmentVisibleExpected?: boolean;
  isSearchSuggestionsFragmentVisibleExpected?: boolean;
  isPreloadFragmentVisibleExpected?: boolean;
}

function assertCardComponentsVisible({
  page,
  isSettingFooterVisibleExpected,
  isBackButtonVisibleExpected,
  isWelcomeFragmentVisibleExpected,
  isCompletionFragmentVisibleExpected,
  isMsbbFragmentVisibleExpected,
  isHistorySyncFragmentVisibleExpected,
  isSafeBrowsingFragmentVisibleExpected,
  isCookiesFragmentVisibleExpected,
  isSearchSuggestionsFragmentVisibleExpected,
  isPreloadFragmentVisibleExpected,
}: AssertCardComponentsVisibleParams) {
  assertEquals(
      !!isSettingFooterVisibleExpected, isChildVisible(page, '#settingFooter'),
      'Incorrect footer visibility.');
  if (isSettingFooterVisibleExpected) {
    const backButtonVisibility =
        getComputedStyle(
            page.shadowRoot!.querySelector<HTMLElement>('#backButton')!)
            .visibility;
    assertEquals(
        isBackButtonVisibleExpected ? 'visible' : 'hidden',
        backButtonVisibility, 'Incorrect back button visibility');
  }
  assertEquals(
      !!isWelcomeFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.WELCOME),
      'Incorrect welcome fragment visibility.');
  assertEquals(
      !!isCompletionFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.COMPLETION),
      'Incorrect completion fragment visibility.');
  assertEquals(
      !!isMsbbFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.MSBB),
      'Incorrect MSBB fragment visibility.');
  assertEquals(
      !!isHistorySyncFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.HISTORY_SYNC),
      'Incorrect history sync fragment visibility.');
  assertEquals(
      !!isSafeBrowsingFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.SAFE_BROWSING),
      'Incorrect SB fragment visibility.');
  assertEquals(
      !!isCookiesFragmentVisibleExpected,
      isChildVisible(page, '#' + PrivacyGuideStep.COOKIES),
      'Incorrect cookies fragment visibility.');
  if (loadTimeData.getBoolean('enablePrivacyGuide3')) {
    assertEquals(
        !!isSearchSuggestionsFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.SEARCH_SUGGESTIONS),
        'Incorrect search suggestions fragment visibility.');
  }
  if (loadTimeData.getBoolean('enablePrivacyGuidePreload')) {
    assertEquals(
        !!isPreloadFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.PRELOAD),
        'Incorrect preloading fragment visibility.');
  }
}

/**
 * @return The expected total number of active cards for the step indicator.
 */
function getExpectedNumberOfActiveCards(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy): number {
  let numSteps = loadTimeData.getBoolean('enablePrivacyGuide3') ?
      PRIVACY_GUIDE_STEPS :
      PRIVACY_GUIDE_STEPS_PG3_OFF;
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    numSteps -= 1;
  }

  // TODO(b:306414714): Remove 3pcd part when 3pcd launched.
  if (!shouldShowCookiesCard(page) ||
      loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
    numSteps -= 1;
  }
  if (!shouldShowSafeBrowsingCard(page)) {
    numSteps -= 1;
  }
  if (loadTimeData.getBoolean('enablePrivacyGuidePreload') &&
      shouldShowPreloadCard(page)) {
    numSteps += 1;
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
  // TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
  if (!loadTimeData.getBoolean('enablePrivacyGuide3')) {
    let activeIndex = 3;
    if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
      activeIndex -= 1;
    }
    if (!shouldShowSafeBrowsingCard(page)) {
      activeIndex -= 1;
    }
    assertStepIndicatorModel(page, syncBrowserProxy, activeIndex);
    return;
  }
  assertStepIndicatorModel(
      page, syncBrowserProxy,
      shouldShowHistorySyncCard(syncBrowserProxy) ? 2 : 1);
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
  // TODO(crbug.com/1215630): Remove PG3 part when PrivacyGuide3 launched.
  // TODO(b:306414714): Remove 3pcd part when 3pcd launched.
  if (!loadTimeData.getBoolean('enablePrivacyGuide3') ||
      loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
    assertStepIndicatorModel(
        page, syncBrowserProxy,
        shouldShowHistorySyncCard(syncBrowserProxy) ? 2 : 1);
    return;
  }
  let activeIndex = 3;
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    activeIndex -= 1;
  }
  if (!shouldShowCookiesCard(page)) {
    activeIndex -= 1;
  }
  assertStepIndicatorModel(page, syncBrowserProxy, activeIndex);
}

function assertSearchSuggestionsCardVisible(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  assertQueryParameter(PrivacyGuideStep.SEARCH_SUGGESTIONS);
  assertCardComponentsVisible({
    page: page,
    isSettingFooterVisibleExpected: true,
    isBackButtonVisibleExpected: true,
    isSearchSuggestionsFragmentVisibleExpected: true,
  });
  let activeIndex = 4;
  // TODO(b:306414714): Remove once 3pcd launched.
  if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
    activeIndex -= 1;
  }
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    activeIndex -= 1;
  }
  if (!shouldShowCookiesCard(page)) {
    activeIndex -= 1;
  }
  if (!shouldShowSafeBrowsingCard(page)) {
    activeIndex -= 1;
  }
  assertStepIndicatorModel(page, syncBrowserProxy, activeIndex);
}

function assertPreloadCardVisible(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy) {
  assertQueryParameter(PrivacyGuideStep.PRELOAD);
  assertCardComponentsVisible({
    page: page,
    isSettingFooterVisibleExpected: true,
    isBackButtonVisibleExpected: true,
    isPreloadFragmentVisibleExpected: true,
  });
  let activeIndex = 5;
  // TODO(b:306414714): Remove once 3pcd launched.
  if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
    activeIndex -= 1;
  }
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    activeIndex -= 1;
  }
  if (!shouldShowCookiesCard(page)) {
    activeIndex -= 1;
  }
  if (!shouldShowSafeBrowsingCard(page)) {
    activeIndex -= 1;
  }
  assertStepIndicatorModel(page, syncBrowserProxy, activeIndex);
}

suite('PrivacyGuidePage', function() {
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
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
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

  test('welcomeCardForwardNavigation', async function() {
    assertFalse(page.getPref('privacy_guide.viewed').value);

    // Navigating to the privacy guide without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    assertWelcomeCardVisible(page);
    assertTrue(page.getPref('privacy_guide.viewed').value);

    // The welcome fragment fires a |start-button-click| event to navigate
    // forward.
    const welcomeFragment = page.shadowRoot!.querySelector<HTMLElement>(
        '#' + PrivacyGuideStep.WELCOME);
    assertTrue(!!welcomeFragment);
    welcomeFragment.dispatchEvent(
        new CustomEvent('start-button-click', {bubbles: true, composed: true}));
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

    // The completion fragment fires a |back-button-click| event to navigate
    // back.
    const completionFragment =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.COMPLETION);
    assertTrue(!!completionFragment);
    completionFragment.dispatchEvent(
        new CustomEvent('back-button-click', {bubbles: true, composed: true}));
    flush();

    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCompletion');
  });

  test('privacyGuideVisibilitySupervisedAccount', async function() {
    // Set the user to have a non-supervised account.
    const syncStatus: SyncStatus = {
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', syncStatus);

    // Navigating to the privacy guide works.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();
    assertWelcomeCardVisible(page);

    // The user signs in to a supervised user account. This hides the privacy
    // guide and navigates away back to privacy settings page.
    const newSyncStatus: SyncStatus = {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    };
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
            .shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup');
    assertTrue(!!sbRadioGroup);
    sbRadioGroup.dispatchEvent(arrowLeftEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    sbRadioGroup.dispatchEvent(arrowRightEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    dispatchArrowRightEvent();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);
    // Forward navigation on the completion card does not trigger a navigation.
    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);

    // Backward flow.
    dispatchArrowLeftEvent();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
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

suite('FlowLength', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);

    return flushTasks();
  });

  test('MSBB_SearchSuggestions', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(page, false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(2, result);
  });

  test('MSBB_HistorySync_SearchSuggestions', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(page, false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(3, result);
  });

  test('MSBB_SafeBrowsing_SearchSuggestions', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(page, true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(3, result);
  });

  test('MSBB_HistorySync_SafeBrowsing_SearchSuggestions', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(page, true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(4, result);
  });
});

// TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
suite('PrivacyGuidePagePG3Off', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({enablePrivacyGuide3: false});

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
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

  test('completionCardBackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.COMPLETION);
    assertCompletionCardVisible(page);

    // The completion fragment fires a |back-button-click| event to navigate
    // back.
    const completionFragment =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.COMPLETION);
    assertTrue(!!completionFragment);
    completionFragment.dispatchEvent(
        new CustomEvent('back-button-click', {bubbles: true, composed: true}));
    flush();

    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCompletion');
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
            .shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup');
    assertTrue(!!sbRadioGroup);
    sbRadioGroup.dispatchEvent(arrowLeftEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    sbRadioGroup.dispatchEvent(arrowRightEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);
    // Forward navigation on the completion card does not trigger a navigation.
    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);

    // Backward flow.
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

// TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
suite('FlowLengthPG3Off', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({enablePrivacyGuide3: false});

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
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

  test('MSBB', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(page, false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(1, result);
  });

  test('MSBB_HistorySync', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(page, false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(2, result);
  });

  test('MSBB_SafeBrowsing', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(page, true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(2, result);
  });

  test('MSBB_HistorySync_SafeBrowsing', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await flushTasks();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(page, true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(3, result);
  });
});

suite('MsbbCardNavigations', function() {
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

  test('BackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertWelcomeCardVisible(page);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickMSBB');
  });

  test('ForwardNavigationSyncOn', async function() {
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

  test('ForwardNavigationSyncOff', async function() {
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

// TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
suite('MsbbCardNavigationsPG3Off', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({enablePrivacyGuide3: false});

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

  test('ForwardNavigationSyncOff', async function() {
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

suite('HistorySyncCardNavigations', function() {
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

  test('BackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickHistorySync');
  });

  test('NavigatesAwayOnSyncOff', async function() {
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

  test('NotReachableWhenSyncOff', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });

  test('ForwardNavigationShouldShowSafeBrowsingCard', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickHistorySync');
  });

  test('ForwardNavigationShouldHideSafeBrowsingCard', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
  });
});

// TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
suite('HistorySyncCardNavigationsPG3Off', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({enablePrivacyGuide3: false});

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

  test('NavigatesAwayOnSyncOff', async function() {
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

  test('NotReachableWhenSyncOff', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });

  test('ForwardNavigationShouldShowSafeBrowsingCard', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickHistorySync');
  });

  test('ForwardNavigationShouldHideSafeBrowsingCard', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible(page);
  });
});

suite('SafeBrowsingCardNavigations', function() {
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

  test('BackNavigationSyncOn', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickSafeBrowsing');
  });

  test('BackNavigationSyncOff', async function() {
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

  test('ForwardNavigationShouldShowSearchSuggestionsCard', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickSafeBrowsing');
  });

  test('safeBrowsingOffNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    // Changing the safe browsing setting to a disabled state while shown should
    // navigate away from the safe browsing card.
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
  });
});

// TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
suite('SafeBrowsingCardNavigationsPG3Off', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({enablePrivacyGuide3: false});

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

  test('BackNavigationSyncOn', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickSafeBrowsing');
  });

  test('BackNavigationSyncOff', async function() {
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

  test('ForwardNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickSafeBrowsing');
  });

  test('safeBrowsingOffNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    // Changing the safe browsing setting to a disabled state while shown should
    // navigate away from the safe browsing card.
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    flush();
    assertCompletionCardVisible(page);
  });
});

suite('CookiesCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({is3pcdCookieSettingsRedesignEnabled: false});
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

  test('BackNavigationShouldShowSync', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCookies');
  });

  test('BackNavigationShouldHideSync', async function() {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertMsbbCardVisible(page, syncBrowserProxy);
  });

  test('ForwardNavigationShouldShowSafeBrowsing', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COOKIES_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCookies');
  });

  test('ForwardNavigationShouldHideSafeBrowsing', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
  });

  test('cookiesAllowAllNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(page, CookiePrimarySetting.ALLOW_ALL);
    await flushTasks();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });

  test('cookiesBlockAllNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(page, CookiePrimarySetting.BLOCK_ALL);
    await flushTasks();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });
});

// TODO(crbug.com/1215630): Remove once PrivacyGuide3 launched.
suite('CookiesCardNavigationsPG3Off', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: false,
      enablePrivacyGuide3: false,
    });
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

  test('NavigationShouldShowSafeBrowsingCard', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCookies');
  });

  test('NavigationShouldHideSafeBrowsingCard', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
  });

  test('ForwardNavigation', async function() {
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

  test('cookiesAllowAllNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(page, CookiePrimarySetting.ALLOW_ALL);
    await flushTasks();
    assertCompletionCardVisible(page);
  });

  test('cookiesBlockAllNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(page, CookiePrimarySetting.BLOCK_ALL);
    await flushTasks();
    assertCompletionCardVisible(page);
  });
});

suite('SearchSuggestionsCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;

  suiteSetup(function() {
    // TODO(b:306414714): Remove once 3pcd launched.
    loadTimeData.overrideValues({is3pcdCookieSettingsRedesignEnabled: true});

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

  test('BackNavigationSafeBrowsingOn', async function() {
    await navigateToStep(PrivacyGuideStep.SEARCH_SUGGESTIONS);
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        actionResult, 'Settings.PrivacyGuide.BackClickSearchSuggestions');
  });

  test('BackNavigationSafeBrowsingOff', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.SEARCH_SUGGESTIONS);
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
  });

  test('ForwardNavigationShouldShowCompletion', async function() {
    await navigateToStep(PrivacyGuideStep.SEARCH_SUGGESTIONS);
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(
        PrivacyGuideInteractions.SEARCH_SUGGESTIONS_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        actionResult, 'Settings.PrivacyGuide.NextClickSearchSuggestions');
  });

  test('hatsInformedOnFinish', async function() {
    await navigateToStep(PrivacyGuideStep.SEARCH_SUGGESTIONS);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();

    // HaTS gets triggered if the user navigates to the completion page.
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE, interaction);
  });
});

suite('PreloadCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enablePrivacyGuidePreload: true,
      // TODO(b:306414714): Remove once 3pcd launched.
      is3pcdCookieSettingsRedesignEnabled: true,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
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

  test('BackNavigation', async function() {
    setPreloadSetting(page, NetworkPredictionOptions.STANDARD);
    await navigateToStep(PrivacyGuideStep.PRELOAD);
    assertPreloadCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
  });

  test('preloadExtendedNavigatesAway', async function() {
    setPreloadSetting(page, NetworkPredictionOptions.STANDARD);
    await navigateToStep(PrivacyGuideStep.PRELOAD);
    assertPreloadCardVisible(page, syncBrowserProxy);

    // Changing preload settings to extended while shown should navigate away.
    setPreloadSetting(page, NetworkPredictionOptions.EXTENDED);
    await flushTasks();
    assertCompletionCardVisible(page);
  });

  test('ForwardNavigationShouldShowCompletion', async function() {
    setPreloadSetting(page, NetworkPredictionOptions.STANDARD);
    await navigateToStep(PrivacyGuideStep.PRELOAD);
    assertPreloadCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible(page);
  });

  test('hatsInformedOnFinish', async function() {
    setPreloadSetting(page, NetworkPredictionOptions.STANDARD);
    await navigateToStep(PrivacyGuideStep.PRELOAD);

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();

    // HaTS gets triggered if the user navigates to the completion page.
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE, interaction);
  });
});

suite('PrivacyGuideDialog', function() {
  let page: SettingsPrivacyGuideDialogElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({is3pcdCookieSettingsRedesignEnabled: true});
  });

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

// TODO(b:306414714): Remove once 3pcd launched.
suite('3pcdOff', function() {
  let page: SettingsPrivacyGuidePageElement;
  let settingsPrefs: SettingsPrefsElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: false,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = createPrivacyGuidePageForTest(settingsPrefs);
    setupPrivacyGuidePageForTest(page, syncBrowserProxy);

    return flushTasks();
  });

  test(
      'length_MSBB_Cookies_SafeBrowsing_SearchSuggestions', async function() {
        Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
        await flushTasks();

        setParametersForHistorySyncStep(syncBrowserProxy, false);
        setParametersForCookiesStep(page, true);
        setParametersForSafeBrowsingStep(page, true);

        await clickNextOnWelcomeStep(page);

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideFlowLengthHistogram');
        assertEquals(4, result);
      });

  test(
      'length_MSBB_HistorySync_Cookies_SafeBrowsing_SearchSuggestions',
      async function() {
        Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
        await flushTasks();

        setParametersForHistorySyncStep(syncBrowserProxy, true);
        setParametersForCookiesStep(page, true);
        setParametersForSafeBrowsingStep(page, true);

        await clickNextOnWelcomeStep(page);

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideFlowLengthHistogram');
        assertEquals(5, result);
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
    assertCookiesCardVisible(page, syncBrowserProxy);
    // Arrow keys don't trigger a navigation when the focus is inside the radio
    // group.
    const cookiesRadioGroup =
        page.shadowRoot!
            .querySelector<HTMLElement>('#' + PrivacyGuideStep.COOKIES)!
            .shadowRoot!.querySelector<HTMLElement>('#cookiesRadioGroup');
    assertTrue(!!cookiesRadioGroup);
    cookiesRadioGroup.dispatchEvent(arrowLeftEvent);
    assertCookiesCardVisible(page, syncBrowserProxy);
    cookiesRadioGroup.dispatchEvent(arrowRightEvent);
    assertCookiesCardVisible(page, syncBrowserProxy);

    dispatchArrowRightEvent();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    // Arrow keys don't trigger a navigation when the focus is inside the radio
    // group.
    const sbRadioGroup =
        page.shadowRoot!
            .querySelector<HTMLElement>('#' + PrivacyGuideStep.SAFE_BROWSING)!
            .shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup');
    assertTrue(!!sbRadioGroup);
    sbRadioGroup.dispatchEvent(arrowLeftEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    sbRadioGroup.dispatchEvent(arrowRightEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    dispatchArrowRightEvent();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);
    // Forward navigation on the completion card does not trigger a navigation.
    dispatchArrowRightEvent();
    assertCompletionCardVisible(page);

    // Backward flow.
    dispatchArrowLeftEvent();
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);
    dispatchArrowLeftEvent();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    dispatchArrowLeftEvent();
    assertCookiesCardVisible(page, syncBrowserProxy);
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
    assertCookiesCardVisible(page, syncBrowserProxy);
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
    assertCookiesCardVisible(page, syncBrowserProxy);
  });

  test('historySyncNotReachableWhenSyncOff', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertCookiesCardVisible(page, syncBrowserProxy);
  });

  test(
      'historySyncCardForwardNavigationShouldShowCookiesCard',
      async function() {
        await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
        assertHistorySyncCardVisible(page, syncBrowserProxy);

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertCookiesCardVisible(page, syncBrowserProxy);

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideNextNavigationHistogram');
        assertEquals(PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON, result);

        const actionResult =
            await testMetricsBrowserProxy.whenCalled('recordAction');
        assertEquals(
            actionResult, 'Settings.PrivacyGuide.NextClickHistorySync');
      });

  test(
      'historySyncCardForwardNavigationShouldHideCookiesCard',
      async function() {
        setCookieSetting(page, CookiePrimarySetting.ALLOW_ALL);
        await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
        assertHistorySyncCardVisible(page, syncBrowserProxy);

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertSafeBrowsingCardVisible(page, syncBrowserProxy);
      });

  test('safeBrowsingCardBackNavigationCookiesOn', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertCookiesCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickSafeBrowsing');
  });

  test('safeBrowsingCardBackNavigationCookiesOff', async function() {
    setCookieSetting(page, CookiePrimarySetting.BLOCK_ALL);
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
  });

  test('searchSuggestionsCardBackNavigationSafeBrowsingOff', async function() {
    setSafeBrowsingSetting(page, SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.SEARCH_SUGGESTIONS);
    assertSearchSuggestionsCardVisible(page, syncBrowserProxy);

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertCookiesCardVisible(page, syncBrowserProxy);
  });
});
