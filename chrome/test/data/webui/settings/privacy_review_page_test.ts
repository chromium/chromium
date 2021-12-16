// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyReviewHistorySyncFragmentElement, PrivacyReviewStep, PrivacyReviewWelcomeFragmentElement, SafeBrowsingSetting, SettingsCheckboxElement, SettingsPrivacyReviewPageElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {Router, routes, StatusAction, SyncBrowserProxyImpl, SyncPrefs, syncPrefsIndividualDataTypes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

/* Maximum number of steps in the privacy review, excluding the welcome and
 * completion steps.
 */
const PRIVACY_REVIEW_STEPS = 4;

suite('PrivacyReviewPage', function() {
  let page: SettingsPrivacyReviewPageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let shouldShowCookiesCard: boolean;
  let shouldShowSafeBrowsingCard: boolean;

  setup(function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-review-page');
    page.disableAnimationsForTesting();
    page.prefs = {
      privacy_review: {
        show_welcome_card: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      privacy_guide: {
        viewed: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      generated: {
        cookie_primary_setting: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookiePrimarySetting.BLOCK_THIRD_PARTY,
        },
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
    };
    document.body.appendChild(page);
    shouldShowCookiesCard = true;
    shouldShowSafeBrowsingCard = true;

    // Simulates the route of the user entering the privacy review from the S&P
    // settings. This is necessary as tests seem to by default define the
    // previous route as Settings "/". On a back navigation, "/" matches the
    // criteria for a valid Settings parent no matter how deep the subpage is in
    // the Settings tree. This would always navigate to Settings "/" instead of
    // to the parent of the current subpage.
    Router.getInstance().navigateTo(routes.PRIVACY);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // Reset route to default. The route is updated as we navigate through the
    // cards, but the browser instance is shared among the tests, so otherwise
    // the next test will be initialized to the same card as the previous test.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  /**
   * Returns a new promise that resolves after a window 'popstate' event.
   */
  function whenPopState(causeEvent: () => void): Promise<void> {
    const promise = eventToPromise('popstate', window);
    causeEvent();
    return promise;
  }

  /**
   * Equivalent of the user manually navigating to the corresponding step via
   * typing the URL and step parameter in the Omnibox.
   */
  function navigateToStep(step: PrivacyReviewStep) {
    Router.getInstance().navigateTo(
        routes.PRIVACY_REVIEW,
        /* opt_dynamicParameters */ new URLSearchParams('step=' + step));
    flush();
  }

  function assertQueryParameter(step: PrivacyReviewStep) {
    assertEquals(step, Router.getInstance().getQueryParameters().get('step'));
  }

  /**
   * Fire a sync status changed event and flush the UI.
   */
  function setSyncEnabled(syncOn: boolean) {
    syncBrowserProxy.testSyncStatus = {
      signedIn: syncOn,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback(
        'sync-status-changed', syncBrowserProxy.testSyncStatus);
    flush();
  }

  function shouldShowHistorySyncCard(): boolean {
    return !syncBrowserProxy.testSyncStatus ||
        !!syncBrowserProxy.testSyncStatus.signedIn;
  }

  /**
   * Set the cookies setting for the privacy review.
   */
  function setCookieSetting(setting: CookiePrimarySetting) {
    page.set('prefs.generated.cookie_primary_setting', {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: setting,
    });
    shouldShowCookiesCard =
        setting === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
        setting === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
    flush();
  }

  /**
   * Set the safe browsing setting for the privacy review.
   */
  function setSafeBrowsingSetting(setting: SafeBrowsingSetting) {
    page.set('prefs.generated.safe_browsing', {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: setting,
    });
    shouldShowSafeBrowsingCard = setting === SafeBrowsingSetting.ENHANCED ||
        setting === SafeBrowsingSetting.STANDARD;
    flush();
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

  type AssertCardComponentsVisibleParams = {
    isSettingFooterVisibleExpected?: boolean,
    isBackButtonVisibleExpected?: boolean,
    isWelcomeFragmentVisibleExpected?: boolean,
    isCompletionFragmentVisibleExpected?: boolean,
    isMsbbFragmentVisibleExpected?: boolean,
    isClearOnExitFragmentVisibleExpected?: boolean,
    isHistorySyncFragmentVisibleExpected?: boolean,
    isSafeBrowsingFragmentVisibleExpected?: boolean,
    isCookiesFragmentVisibleExpected?: boolean,
  };

  function assertCardComponentsVisible({
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
        !!isSettingFooterVisibleExpected,
        isChildVisible(page, '#settingFooter'));
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
        isChildVisible(page, '#' + PrivacyReviewStep.WELCOME));
    assertEquals(
        !!isCompletionFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyReviewStep.COMPLETION));
    assertEquals(
        !!isMsbbFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyReviewStep.MSBB));
    assertEquals(
        !!isClearOnExitFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyReviewStep.CLEAR_ON_EXIT));
    assertEquals(
        !!isHistorySyncFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyReviewStep.HISTORY_SYNC));
    assertEquals(
        !!isSafeBrowsingFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyReviewStep.SAFE_BROWSING));
    assertEquals(
        !!isCookiesFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyReviewStep.COOKIES));
  }

  /**
   * @return The expected total number of active cards for the step indicator.
   */
  function getExpectedNumberOfActiveCards() {
    let numSteps = PRIVACY_REVIEW_STEPS;
    if (!shouldShowHistorySyncCard()) {
      numSteps -= 1;
    }
    if (!shouldShowCookiesCard) {
      numSteps -= 1;
    }
    if (!shouldShowSafeBrowsingCard) {
      numSteps -= 1;
    }
    return numSteps;
  }

  function assertStepIndicatorModel(activeIndex: number) {
    const model = page.computeStepIndicatorModel();
    assertEquals(activeIndex, model.active);
    assertEquals(getExpectedNumberOfActiveCards(), model.total);
  }

  function assertWelcomeCardVisible() {
    assertQueryParameter(PrivacyReviewStep.WELCOME);
    assertCardComponentsVisible({
      isWelcomeFragmentVisibleExpected: true,
    });
  }

  function assertCompletionCardVisible() {
    assertQueryParameter(PrivacyReviewStep.COMPLETION);
    assertCardComponentsVisible({
      isCompletionFragmentVisibleExpected: true,
    });
  }

  function assertMsbbCardVisible() {
    assertQueryParameter(PrivacyReviewStep.MSBB);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isMsbbFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(0);
  }

  function assertHistorySyncCardVisible() {
    assertQueryParameter(PrivacyReviewStep.HISTORY_SYNC);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isHistorySyncFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(1);
  }

  function assertSafeBrowsingCardVisible() {
    assertQueryParameter(PrivacyReviewStep.SAFE_BROWSING);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isSafeBrowsingFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(shouldShowHistorySyncCard() ? 2 : 1);
  }

  function assertCookiesCardVisible() {
    assertQueryParameter(PrivacyReviewStep.COOKIES);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isCookiesFragmentVisibleExpected: true,
    });
    let activeIndex = 3;
    if (!shouldShowHistorySyncCard()) {
      activeIndex -= 1;
    }
    if (!shouldShowSafeBrowsingCard) {
      activeIndex -= 1;
    }
    assertStepIndicatorModel(activeIndex);
  }

  test('startPrivacyReview', function() {
    // Make sure the pref to show the welcome card is on.
    page.setPrefValue('privacy_review.show_welcome_card', true);

    // Navigating to the privacy review without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    flush();
    assertWelcomeCardVisible();

    assertTrue(page.getPref('privacy_guide.viewed').value);

    const welcomeFragment =
        page.shadowRoot!.querySelector<PrivacyReviewWelcomeFragmentElement>(
            '#' + PrivacyReviewStep.WELCOME)!;
    const dontShowAgainCheckbox =
        welcomeFragment.shadowRoot!.querySelector<SettingsCheckboxElement>(
            '#dontShowAgainCheckbox')!;
    assertFalse(dontShowAgainCheckbox.checked);
    dontShowAgainCheckbox.$.checkbox.click();
    welcomeFragment.$.startButton.click();
    flush();
    assertMsbbCardVisible();

    // Navigating this time should skip the welcome card.
    assertFalse(page.getPref('privacy_review.show_welcome_card').value);
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    assertMsbbCardVisible();
  });

  test('welcomeForwardNavigation', function() {
    page.setPrefValue('privacy_review.show_welcome_card', true);

    // Navigating to the privacy review without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    flush();
    assertWelcomeCardVisible();

    const welcomeFragment =
        page.shadowRoot!.querySelector<PrivacyReviewWelcomeFragmentElement>(
            '#' + PrivacyReviewStep.WELCOME)!;
    welcomeFragment.$.startButton.click();
    flush();
    assertMsbbCardVisible();

    setSyncEnabled(true);
    assertMsbbCardVisible();
  });

  test('msbbForwardNavigationSyncOn', function() {
    navigateToStep(PrivacyReviewStep.MSBB);
    setSyncEnabled(true);
    assertMsbbCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertHistorySyncCardVisible();
  });

  test('msbbForwardNavigationSyncOff', function() {
    navigateToStep(PrivacyReviewStep.MSBB);
    setSyncEnabled(false);
    assertMsbbCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertSafeBrowsingCardVisible();
  });

  test('historySyncBackNavigation', function() {
    navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible();
  });

  test('historySyncNavigatesAwayOnSyncOff', function() {
    navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    // User disables sync while history sync card is shown.
    setSyncEnabled(false);
    assertSafeBrowsingCardVisible();
  });

  test('historySyncNotReachableWhenSyncOff', function() {
    navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
    setSyncEnabled(false);
    assertSafeBrowsingCardVisible();
  });

  test(
      'historySyncCardForwardNavigationShouldShowSafeBrowsingCard', function() {
        navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
        setSyncEnabled(true);
        setSafeBrowsingSetting(SafeBrowsingSetting.ENHANCED);
        setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
        assertHistorySyncCardVisible();

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertSafeBrowsingCardVisible();
      });

  test(
      'historySyncCardForwardNavigationShouldHideSafeBrowsingCard', function() {
        navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
        setSyncEnabled(true);
        setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
        setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
        assertHistorySyncCardVisible();

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertCookiesCardVisible();
      });

  test('safeBrowsingCardBackNavigationSyncOn', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setSyncEnabled(true);
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible();
  });

  test('safeBrowsingCardBackNavigationSyncOff', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setSyncEnabled(false);
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible();
  });

  test('safeBrowsingCardGetsUpdated', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setSafeBrowsingSetting(SafeBrowsingSetting.ENHANCED);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertSafeBrowsingCardVisible();
    const radioButtonGroup =
        page.shadowRoot!.querySelector('#' + PrivacyReviewStep.SAFE_BROWSING)!
            .shadowRoot!.querySelector<SettingsRadioGroupElement>(
                '#safeBrowsingRadioGroup')!;
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.ENHANCED);

    // Changing the safe browsing setting should automatically change the
    // selected radio button.
    setSafeBrowsingSetting(SafeBrowsingSetting.STANDARD);
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.STANDARD);

    // Changing the safe browsing setting to a disabled state while shown should
    // navigate away from the safe browsing card.
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    assertCookiesCardVisible();
  });

  test('safeBrowsingCardForwardNavigationShouldShowCookiesCard', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCookiesCardVisible();
  });

  test('safeBrowsingCardForwardNavigationShouldHideCookiesCard', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setCookieSetting(CookiePrimarySetting.ALLOW_ALL);
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardBackNavigationShouldShowSafeBrowsingCard', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    setSyncEnabled(true);
    setSafeBrowsingSetting(SafeBrowsingSetting.STANDARD);
    assertCookiesCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertSafeBrowsingCardVisible();
  });

  test('cookiesCardBackNavigationShouldHideSafeBrowsingCard', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    setSyncEnabled(true);
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    assertCookiesCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('cookiesCardForwardNavigation', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    assertCookiesCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardGetsUpdated', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertCookiesCardVisible();
    const radioButtonGroup =
        page.shadowRoot!.querySelector('#' + PrivacyReviewStep.COOKIES)!
            .shadowRoot!.querySelector<SettingsRadioGroupElement>(
                '#cookiesRadioGroup')!;
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY);

    // Changing the cookie setting should automatically change the selected
    // radio button.
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(CookiePrimarySetting.ALLOW_ALL);
    assertCompletionCardVisible();
  });

  test('completionCardBackNavigation', function() {
    navigateToStep(PrivacyReviewStep.COMPLETION);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertCompletionCardVisible();

    const completionFragment =
        page.shadowRoot!.querySelector('#' + PrivacyReviewStep.COMPLETION)!;
    completionFragment.shadowRoot!.querySelector<HTMLElement>(
                                      '#backButton')!.click();
    flush();
    assertCookiesCardVisible();
  });

  test('completionCardBackToSettingsNavigation', function() {
    navigateToStep(PrivacyReviewStep.COMPLETION);
    assertCompletionCardVisible();

    return whenPopState(function() {
             const completionFragment = page.shadowRoot!.querySelector(
                 '#' + PrivacyReviewStep.COMPLETION)!;
             completionFragment.shadowRoot!
                 .querySelector<HTMLElement>('#leaveButton')!.click();
           })
        .then(function() {
          assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
        });
  });

  test('completionCardGetsUpdated', function() {
    navigateToStep(PrivacyReviewStep.COMPLETION);
    setSignInState(true);
    assertCompletionCardVisible();

    const completionFragment =
        page.shadowRoot!.querySelector('#' + PrivacyReviewStep.COMPLETION)!;
    assertTrue(isChildVisible(completionFragment, '#privacySandboxRow'));
    assertTrue(isChildVisible(completionFragment, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(completionFragment, '#privacySandboxRow'));
    assertFalse(isChildVisible(completionFragment, '#waaRow'));
  });
});

suite('HistorySyncFragment', function() {
  let page: PrivacyReviewHistorySyncFragmentElement;
  let syncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('privacy-review-history-sync-fragment');
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
    flush();
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

    // Re-enabling history sync in the privacy review doesn't re-enable sync
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
        routes.PRIVACY_REVIEW,
        /* opt_dynamicParameters */ new URLSearchParams('step=msbb'));
    Router.getInstance().navigateTo(
        routes.PRIVACY_REVIEW,
        /* opt_dynamicParameters */ new URLSearchParams('step=historySync'));

    // Re-enabling history sync in the privacy review doesn't re-enable sync
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
