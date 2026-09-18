// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsPrivacyGuideDialogElement, SettingsPrivacyGuidePageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, CookieControlsMode, PrivacyGuideStep, SafeBrowsingSetting, ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
import type {SyncStatus} from 'chrome://settings/settings.js';
import {HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, PrefService, PrefsBrowserProxy, PrivacyGuideInteractions, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createPrivacyGuidePageForTest, getInitialPrivacyGuideTestPrefs, navigateToStep, clickNextOnWelcomeStep, setFirstPartyCookieSetting, setThirdPartyCookieBlockingSetting, setThirdPartyCookieSetting, setParametersForCookiesStep, setParametersForHistorySyncStep, setParametersForSafeBrowsingStep, setSafeBrowsingSetting, setupPrivacyRouteForTest, setupSync, shouldShowCookiesCard, shouldShowHistorySyncCard, shouldShowSafeBrowsingCard} from './privacy_guide_test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';

// clang-format on

// Maximum number of steps in the privacy guide, excluding the welcome and
// completion steps.
const PRIVACY_GUIDE_STEPS = 4;

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
}: AssertCardComponentsVisibleParams) {
  assertEquals(
      !!isSettingFooterVisibleExpected, isChildVisible(page, '#settingFooter'),
      'Incorrect footer visibility.');
  if (isSettingFooterVisibleExpected) {
    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    const backButtonVisibility = getComputedStyle(backButton).visibility;
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
}

/**
 * @return The expected total number of active cards for the step indicator.
 */
function getExpectedNumberOfActiveCards(
    syncBrowserProxy: TestSyncBrowserProxy): number {
  let numSteps = PRIVACY_GUIDE_STEPS;
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    numSteps -= 1;
  }

  if (!shouldShowCookiesCard()) {
    numSteps -= 1;
  }
  if (!shouldShowSafeBrowsingCard()) {
    numSteps -= 1;
  }
  return numSteps;
}

function assertStepIndicatorModel(
    page: SettingsPrivacyGuidePageElement,
    syncBrowserProxy: TestSyncBrowserProxy,
    activeIndex: number) {
  const model = page.computeStepIndicatorModel();
  assertEquals(activeIndex, model.active);
  assertEquals(getExpectedNumberOfActiveCards(syncBrowserProxy), model.total);
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
  let activeIndex = 3;
  if (!shouldShowHistorySyncCard(syncBrowserProxy)) {
    activeIndex -= 1;
  }
  if (!shouldShowSafeBrowsingCard()) {
    activeIndex -= 1;
  }
  assertStepIndicatorModel(page, syncBrowserProxy, activeIndex);
  return;
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



suite('PrivacyGuidePage', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = await createPrivacyGuidePageForTest(syncBrowserProxy);

    return microtasksFinished();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('welcomeCardForwardNavigation', async function() {
    assertFalse(PrefService.getInstance()
                    .getPref<boolean>('privacy_guide.viewed')
                    .value);

    // Navigating to the privacy guide without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    assertWelcomeCardVisible(page);
    assertTrue(PrefService.getInstance()
                   .getPref<boolean>('privacy_guide.viewed')
                   .value);

    // The welcome fragment fires a |start-button-click| event to navigate
    // forward.
    const welcomeFragment = page.shadowRoot.querySelector<HTMLElement>(
        '#' + PrivacyGuideStep.WELCOME);
    assertTrue(!!welcomeFragment);
    welcomeFragment.dispatchEvent(
        new CustomEvent('start-button-click', {bubbles: true, composed: true}));
    await microtasksFinished();

    assertMsbbCardVisible(page, syncBrowserProxy);
    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.WELCOME_NEXT_BUTTON, result);
    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickWelcome');

    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      signedInState: SignedInState.SYNCING,
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
        page.shadowRoot.querySelector('#' + PrivacyGuideStep.COMPLETION);
    assertTrue(!!completionFragment);
    completionFragment.dispatchEvent(
        new CustomEvent('back-button-click', {bubbles: true, composed: true}));
    await microtasksFinished();

    assertCookiesCardVisible(page, syncBrowserProxy);
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
    await microtasksFinished();
    assertWelcomeCardVisible(page);

    // The user signs in to a supervised user account. This hides the privacy
    // guide and navigates away back to privacy settings page.
    const newSyncStatus: SyncStatus = {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', newSyncStatus);
    await microtasksFinished();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());

    // User trying to manually navigate to privacy guide fails.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('privacyGuideVisibilityManagedAccount', async function() {
    // Set the user to have a non-managed account.
    webUIListenerCallback('is-managed-changed', false);

    // Navigating to the privacy guide works.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();
    assertWelcomeCardVisible(page);

    // The user signs in to a managed account. This hides the privacy guide and
    // navigates away back to privacy settings page.
    webUIListenerCallback('is-managed-changed', true);
    await microtasksFinished();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());

    // User trying to manually navigate to privacy guide fails.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('arrowKeyNavigation', async function() {
    const pgCard =
        page.shadowRoot.querySelector<HTMLElement>('#privacyGuideCard');
    const arrowLeftEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});
    const arrowRightEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});
    async function dispatchArrowLeftEvent() {
      assertTrue(!!pgCard);
      pgCard.dispatchEvent(arrowLeftEvent);
      await microtasksFinished();
    }
    async function dispatchArrowRightEvent() {
      assertTrue(!!pgCard);
      pgCard.dispatchEvent(arrowRightEvent);
      await microtasksFinished();
    }

    // Ensure a defined text direction.
    loadTimeData.overrideValues({textdirection: 'ltr'});

    // Forward flow.
    await navigateToStep(PrivacyGuideStep.WELCOME);
    await dispatchArrowRightEvent();
    assertMsbbCardVisible(page, syncBrowserProxy);
    await dispatchArrowRightEvent();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
    await dispatchArrowRightEvent();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    // Arrow keys don't trigger a navigation when the focus is inside the radio
    // group.
    const safeBrowsingFragment = page.shadowRoot.querySelector<HTMLElement>(
        '#' + PrivacyGuideStep.SAFE_BROWSING);
    assertTrue(!!safeBrowsingFragment);
    const sbRadioGroup =
        safeBrowsingFragment.shadowRoot!.querySelector<HTMLElement>(
            '#safeBrowsingRadioGroup');
    assertTrue(!!sbRadioGroup);
    sbRadioGroup.dispatchEvent(arrowLeftEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    sbRadioGroup.dispatchEvent(arrowRightEvent);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    await dispatchArrowRightEvent();
    assertCookiesCardVisible(page, syncBrowserProxy);
    // Arrow keys don't trigger a navigation when the focus is inside the radio
    // group.
    const cookiesFragment = page.shadowRoot.querySelector<HTMLElement>(
        '#' + PrivacyGuideStep.COOKIES);
    assertTrue(!!cookiesFragment);
    const cookiesRadioGroup =
        cookiesFragment.shadowRoot!.querySelector<HTMLElement>(
            '#cookiesRadioGroup');
    assertTrue(!!cookiesRadioGroup);
    cookiesRadioGroup.dispatchEvent(arrowLeftEvent);
    assertCookiesCardVisible(page, syncBrowserProxy);
    cookiesRadioGroup.dispatchEvent(arrowRightEvent);
    assertCookiesCardVisible(page, syncBrowserProxy);

    await dispatchArrowRightEvent();
    assertCompletionCardVisible(page);
    // Forward navigation on the completion card does not trigger a navigation.
    await dispatchArrowRightEvent();
    assertCompletionCardVisible(page);

    // Backward flow.
    await dispatchArrowLeftEvent();
    assertCookiesCardVisible(page, syncBrowserProxy);
    await dispatchArrowLeftEvent();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    await dispatchArrowLeftEvent();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
    await dispatchArrowLeftEvent();
    assertMsbbCardVisible(page, syncBrowserProxy);
    await dispatchArrowLeftEvent();
    assertWelcomeCardVisible(page);
    // Backward navigation on the welcome card does not trigger a navigation.
    await dispatchArrowLeftEvent();
    assertWelcomeCardVisible(page);
  });
});

suite('FlowLength', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = await createPrivacyGuidePageForTest(syncBrowserProxy);

    return microtasksFinished();
  });

  test('MSBB', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(false);
    setParametersForCookiesStep(false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(1, result);
  });

  test('MSBB_HistorySync', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(false);
    setParametersForCookiesStep(false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(2, result);
  });

  test('MSBB_Cookies', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(false);
    setParametersForCookiesStep(true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(2, result);
  });

  test('MSBB_Cookies_HistorySync', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(3, result);
  });

  test('MSBB_Cookies_SafeBrowsing', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForCookiesStep(true);
    setParametersForSafeBrowsingStep(true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(3, result);
  });

  test('MSBB_HistorySync_Cookies_SafeBrowsing', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForCookiesStep(true);
    setParametersForSafeBrowsingStep(true);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(4, result);
  });

  test('MSBB_SafeBrowsing', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, false);
    setParametersForSafeBrowsingStep(true);
    setParametersForCookiesStep(false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(2, result);
  });

  test('MSBB_HistorySync_SafeBrowsing', async function() {
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    await microtasksFinished();

    setParametersForHistorySyncStep(syncBrowserProxy, true);
    setParametersForSafeBrowsingStep(true);
    setParametersForCookiesStep(false);

    await clickNextOnWelcomeStep(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideFlowLengthHistogram');
    assertEquals(3, result);
  });
});

suite('MsbbCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = await createPrivacyGuidePageForTest(syncBrowserProxy);

    return microtasksFinished();
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

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    await microtasksFinished();
    assertWelcomeCardVisible(page);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickMSBB');
  });

  test('ForwardNavigationSyncOn', async function() {
    await navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
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
      signedInState: SignedInState.SIGNED_OUT,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    await navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'Settings.PrivacyGuide.NextClickMSBB',
        testMetricsBrowserProxy.getArgs('recordAction')[0]);
  });
});

suite('HistorySyncCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    page = await createPrivacyGuidePageForTest(syncBrowserProxy);

    return microtasksFinished();
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

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
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
      signedInState: SignedInState.SIGNED_OUT,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));
  });

  test('NotReachableWhenSyncOff', async function() {
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      signedInState: SignedInState.SIGNED_OUT,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));
  });

  test('ForwardNavigationShouldHideSafeBrowsingCard', async function() {
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
    await microtasksFinished();
    assertCookiesCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'Settings.PrivacyGuide.NextClickHistorySync',
        testMetricsBrowserProxy.getArgs('recordAction')[0]);
  });

  test('ShownForSignedInUsersWhenFlagEnabled', async function() {
    loadTimeData.overrideValues({'replaceSyncPromosWithSignInPromos': true});

    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      signedInState: SignedInState.SIGNED_IN,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });

    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    assertHistorySyncCardVisible(page, syncBrowserProxy);
  });

  // <if expr="is_chromeos">
  test('NotShownForSignedInUsersWhenFlagDisabled', async function() {
    loadTimeData.overrideValues({'replaceSyncPromosWithSignInPromos': false});

    await navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      signedInState: SignedInState.SIGNED_IN,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    // The history sync card is removed/not shown anymore and the next card is
    // shown instead.
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);
  });
  // </if>
});

suite('SafeBrowsingCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    page = await createPrivacyGuidePageForTest(syncBrowserProxy);

    return microtasksFinished();
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

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    assertHistorySyncCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickSafeBrowsing');
  });

  test('BackNavigationSyncOff', async function() {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      signedInState: SignedInState.SIGNED_OUT,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    assertMsbbCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'Settings.PrivacyGuide.BackClickSafeBrowsing',
        testMetricsBrowserProxy.getArgs('recordAction')[0]);
  });

  test('ForwardNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
    await microtasksFinished();
    assertCookiesCardVisible(page, syncBrowserProxy);

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
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    await microtasksFinished();
    assertCookiesCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));
  });
});

suite('CookiesCardNavigations', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testHatsBrowserProxy: TestHatsBrowserProxy;

  suiteSetup(function() {
    resetRouterForTesting();
  });

  setup(async function() {
    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    page = await createPrivacyGuidePageForTest(syncBrowserProxy);

    return microtasksFinished();
  });

  teardown(function() {
    page.remove();
    // The browser instance is shared among the tests, hence the route needs to
    // be reset between tests.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('BackNavigationShouldShowSafeBrowsing', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    await microtasksFinished();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCookies');
  });

  test('BackNavigationShouldSafeBrowsingSync', async function() {
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    await microtasksFinished();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'Settings.PrivacyGuide.BackClickCookies',
        testMetricsBrowserProxy.getArgs('recordAction')[0]);
  });

  test('ForwardNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
    await microtasksFinished();
    assertCompletionCardVisible(page);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COOKIES_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCookies');
  });

  test('cookiesCardVisibleWhenCookieControlsModeOff', async function() {
    setFirstPartyCookieSetting(ContentSetting.ALLOW);
    setThirdPartyCookieSetting(CookieControlsMode.OFF);
    setThirdPartyCookieBlockingSetting(
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    assertTrue(shouldShowCookiesCard());
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);
  });

  test('cookiesBlockAllNavigatesAway', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    // Blocking first party cookies should navigate away from the cookies card.
    setFirstPartyCookieSetting(ContentSetting.BLOCK);
    await microtasksFinished();
    assertCompletionCardVisible(page);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));
  });

  test('hatsInformedOnFinish', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();

    // HaTS gets triggered if the user navigates to the completion page.
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE, interaction);
  });

  test('safeBrowsingForwardNavigationShouldShowCookies', async function() {
    await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
    assertCookiesCardVisible(page, syncBrowserProxy);

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickSafeBrowsing');
  });

  test(
      'safeBrowsingForwardNavigationShouldShowCookiesWhenOff',
      async function() {
        setThirdPartyCookieSetting(CookieControlsMode.OFF);
        await navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
        assertSafeBrowsingCardVisible(page, syncBrowserProxy);

        const nextButton =
            page.shadowRoot.querySelector<HTMLElement>('#nextButton');
        assertTrue(!!nextButton);
        nextButton.click();
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

  test('cookiesBackNavigationSafeBrowsingOn', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    assertSafeBrowsingCardVisible(page, syncBrowserProxy);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCookies');
  });

  test('cookiesBackNavigationSafeBrowsingOff', async function() {
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    const backButton =
        page.shadowRoot.querySelector<HTMLElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    await microtasksFinished();
    assertHistorySyncCardVisible(page, syncBrowserProxy);
    // Verify user actions are only emitted for available cards on navigation.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'Settings.PrivacyGuide.BackClickCookies',
        testMetricsBrowserProxy.getArgs('recordAction')[0]);
  });

  test('cookiesForwardNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible(page, syncBrowserProxy);

    const nextButton =
        page.shadowRoot.querySelector<HTMLElement>('#nextButton');
    assertTrue(!!nextButton);
    nextButton.click();
    await microtasksFinished();
    assertCompletionCardVisible(page);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCookies');
  });

  test('completionCardBackNavigation', async function() {
    await navigateToStep(PrivacyGuideStep.COMPLETION);
    assertCompletionCardVisible(page);

    // The completion fragment fires a |back-button-click| event to navigate
    // back.
    const completionFragment =
        page.shadowRoot.querySelector('#' + PrivacyGuideStep.COMPLETION);
    assertTrue(!!completionFragment);
    completionFragment.dispatchEvent(
        new CustomEvent('back-button-click', {bubbles: true, composed: true}));
    await microtasksFinished();

    assertCookiesCardVisible(page, syncBrowserProxy);
    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCompletion');
  });
});

suite('PrivacyGuideDialog', function() {
  let page: SettingsPrivacyGuideDialogElement;

  suiteSetup(function() {
    resetRouterForTesting();
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    const prefsBrowserProxy =
        new TestPrefsBrowserProxy(getInitialPrivacyGuideTestPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    page = document.createElement('settings-privacy-guide-dialog');
    document.body.appendChild(page);

    setupPrivacyRouteForTest();

    return microtasksFinished();
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
    const privacyGuidePage = page.shadowRoot.querySelector<HTMLElement>(
        'settings-privacy-guide-page');
    assertTrue(!!privacyGuidePage);
    privacyGuidePage.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));

    assertFalse(page.$.dialog.open);
  });
});
