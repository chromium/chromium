// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyReviewHistorySyncFragmentElement, PrivacyReviewStep, SafeBrowsingSetting, SettingsPrivacyReviewPageElement} from 'chrome://settings/lazy_load.js';
import {Route, Router, routes, SyncBrowserProxyImpl, syncPrefsIndividualDataTypes} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

/* Maximum number of steps in the privacy review, excluding the welcome step. */
const PRIVACY_REVIEW_STEPS = 5;

suite('PrivacyReviewPage', function() {
  /** @type {!SettingsPrivacyReviewPageElement} */
  let page;
  let isSyncOn;
  let shouldShowCookiesCard;
  let shouldShowSafeBrowsingCard;

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyReviewPageElement} */
        (document.createElement('settings-privacy-review-page'));
    page.prefs = {
      privacy_review: {
        show_welcome_card: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
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
    isSyncOn = false;
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
  });

  /**
   * Returns a new promise that resolves after a window 'popstate' event.
   * @return {!Promise}
   */
  function whenPopState(causeEvent) {
    const promise = new Promise(function(resolve) {
      window.addEventListener('popstate', function callback() {
        window.removeEventListener('popstate', callback);
        resolve();
      });
    });

    causeEvent();
    return promise;
  }

  /**
   * Equivalent of the user manually navigating to the corresponding step via
   * typing the URL and step parameter in the Omnibox.
   * @private
   * @param {string} step
   */
  function navigateToStep(step) {
    Router.getInstance().navigateTo(
        routes.PRIVACY_REVIEW,
        /* opt_dynamicParameters */ new URLSearchParams('step=' + step));
    flush();
  }

  /**
   * @param {string} step
   */
  function assertQueryParameter(step) {
    assertEquals(step, Router.getInstance().getQueryParameters().get('step'));
  }


  /**
   * Fire a sync status changed event and flush the UI.
   * @param {boolean} syncOn
   */
  function setSyncEnabled(syncOn) {
    const event = {
      signedIn: syncOn,
      hasError: false,
    };
    webUIListenerCallback('sync-status-changed', event);
    flush();
    isSyncOn = syncOn;
  }

  /**
   * Set the cookies setting for the privacy review.
   * @param {CookiePrimarySetting} setting
   */
  function setCookieSetting(setting) {
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
   * @param {SafeBrowsingSetting} setting
   */
  function setSafeBrowsingSetting(setting) {
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
   * @param {boolean} signedIn
   */
  function setSignInState(signedIn) {
    const event = {
      signedIn: signedIn,
    };
    webUIListenerCallback('update-sync-state', event);
    flush();
  }

  /**
   * @param {!{
   *   headerTextExpected: (string|undefined),
   *   isSettingFooterVisibleExpected: (boolean|undefined),
   *   isBackButtonVisibleExpected: (boolean|undefined),
   *   isWelcomeFragmentVisibleExpected: (boolean|undefined),
   *   isCompletionFragmentVisibleExpected: (boolean|undefined),
   *   isMsbbFragmentVisibleExpected: (boolean|undefined),
   *   isClearOnExitFragmentVisibleExpected: (boolean|undefined),
   *   isHistorySyncFragmentVisibleExpected: (boolean|undefined),
   *   isSafeBrowsingFragmentVisibleExpected: (boolean|undefined),
   *   isCookiesFragmentVisibleExpected: (boolean|undefined),
   * }} destructured1
   */
  function assertCardComponentsVisible({
    headerTextExpected,
    isSettingFooterVisibleExpected,
    isBackButtonVisibleExpected,
    isWelcomeFragmentVisibleExpected,
    isCompletionFragmentVisibleExpected,
    isMsbbFragmentVisibleExpected,
    isClearOnExitFragmentVisibleExpected,
    isHistorySyncFragmentVisibleExpected,
    isSafeBrowsingFragmentVisibleExpected,
    isCookiesFragmentVisibleExpected,
  }) {
    assertEquals(!!headerTextExpected, isChildVisible(page, '#header'));
    if (headerTextExpected) {
      assertEquals(
          headerTextExpected,
          page.shadowRoot.querySelector('#headerLabel').innerText);
    }
    assertEquals(
        !!isSettingFooterVisibleExpected,
        isChildVisible(page, '#settingFooter'));
    if (isSettingFooterVisibleExpected) {
      const backButtonVisibility =
          getComputedStyle(page.shadowRoot.querySelector('#backButton'))
              .visibility;
      assertEquals(
          isBackButtonVisibleExpected ? 'visible' : 'hidden',
          backButtonVisibility);
    }
    assertEquals(
        !!isWelcomeFragmentVisibleExpected,
        isChildVisible(page, '#welcomeFragment'));
    assertEquals(
        !!isCompletionFragmentVisibleExpected,
        isChildVisible(page, '#completionFragment'));
    assertEquals(
        !!isMsbbFragmentVisibleExpected, isChildVisible(page, '#msbbFragment'));
    assertEquals(
        !!isClearOnExitFragmentVisibleExpected,
        isChildVisible(page, '#clearOnExitFragment'));
    assertEquals(
        !!isHistorySyncFragmentVisibleExpected,
        isChildVisible(page, '#historySyncFragment'));
    assertEquals(
        !!isSafeBrowsingFragmentVisibleExpected,
        isChildVisible(page, '#safeBrowsingFragment'));
    assertEquals(
        !!isCookiesFragmentVisibleExpected,
        isChildVisible(page, '#cookiesFragment'));
  }

  /**
   * @return The expected total number of active cards for the step indicator.
   */
  function getExpectedNumberOfActiveCards() {
    let numSteps = PRIVACY_REVIEW_STEPS;
    if (!isSyncOn) {
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

  /**
   * @param {number} activeIndex
   */
  function assertStepIndicatorModel(activeIndex) {
    const model = page.computeStepIndicatorModel_();
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
      headerTextExpected: page.i18n('privacyReviewMsbbCardHeader'),
      isSettingFooterVisibleExpected: true,
      isMsbbFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(0);
  }

  function assertClearOnExitCardVisible() {
    assertQueryParameter(PrivacyReviewStep.CLEAR_ON_EXIT);
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewClearOnExitCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isClearOnExitFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(1);
  }

  function assertHistorySyncCardVisible() {
    assertQueryParameter(PrivacyReviewStep.HISTORY_SYNC);
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewHistorySyncCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isHistorySyncFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(1);
  }

  function assertSafeBrowsingCardVisible() {
    assertQueryParameter(PrivacyReviewStep.SAFE_BROWSING);
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewSafeBrowsingCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isSafeBrowsingFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(isSyncOn ? 2 : 1);
  }

  function assertCookiesCardVisible() {
    assertQueryParameter(PrivacyReviewStep.COOKIES);
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewCookiesCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isCookiesFragmentVisibleExpected: true,
    });
    let activeIndex = 3;
    if (!isSyncOn) {
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
    flush();

    // Navigating to the privacy review without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    flush();
    assertWelcomeCardVisible();

    const welcomeFragment = page.shadowRoot.querySelector('#welcomeFragment');
    const dontShowAgainCheckbox =
        welcomeFragment.shadowRoot.querySelector('#dontShowAgainCheckbox');
    assertFalse(dontShowAgainCheckbox.checked);
    dontShowAgainCheckbox.$.checkbox.click();
    welcomeFragment.shadowRoot.querySelector('#startButton').click();
    flush();
    assertMsbbCardVisible();

    // Navigating this time should skip the welcome card.
    assertFalse(page.getPref('privacy_review.show_welcome_card').value);
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    flush();
    assertMsbbCardVisible();
  });

  test('welcomeForwardNavigation', function() {
    page.setPrefValue('privacy_review.show_welcome_card', true);
    flush();

    // Navigating to the privacy review without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    flush();
    assertWelcomeCardVisible();

    const welcomeFragment = page.shadowRoot.querySelector('#welcomeFragment');
    welcomeFragment.shadowRoot.querySelector('#startButton').click();
    flush();
    assertMsbbCardVisible();

    setSyncEnabled(true);
    flush();
    assertMsbbCardVisible();
  });

  test('msbbForwardNavigationSyncOn', function() {
    navigateToStep(PrivacyReviewStep.MSBB);
    setSyncEnabled(true);
    assertMsbbCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('msbbForwardNavigationSyncOff', function() {
    navigateToStep(PrivacyReviewStep.MSBB);
    setSyncEnabled(false);
    assertMsbbCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertSafeBrowsingCardVisible();
  });

  test('historySyncBackNavigation', function() {
    navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
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

        page.shadowRoot.querySelector('#nextButton').click();
        flush();
        assertSafeBrowsingCardVisible();
      });

  test(
      'historySyncCardForwardNavigationShouldHideSafeBrowsingCard', function() {
        navigateToStep(PrivacyReviewStep.HISTORY_SYNC);
        setSyncEnabled(true);
        setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
        setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
        assertHistorySyncCardVisible();

        page.shadowRoot.querySelector('#nextButton').click();
        flush();
        assertCookiesCardVisible();
      });

  test('safeBrowsingCardBackNavigationSyncOn', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setSyncEnabled(true);
    assertSafeBrowsingCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('safeBrowsingCardBackNavigationSyncOff', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setSyncEnabled(false);
    assertSafeBrowsingCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertMsbbCardVisible();
  });

  test('safeBrowsingCardGetsUpdated', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setSafeBrowsingSetting(SafeBrowsingSetting.ENHANCED);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertSafeBrowsingCardVisible();
    const radioButtonGroup =
        page.shadowRoot.querySelector('#safeBrowsingFragment')
            .shadowRoot.querySelector('#safeBrowsingRadioGroup');
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

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCookiesCardVisible();
  });

  test('safeBrowsingCardForwardNavigationShouldHideCookiesCard', function() {
    navigateToStep(PrivacyReviewStep.SAFE_BROWSING);
    setCookieSetting(CookiePrimarySetting.ALLOW_ALL);
    assertSafeBrowsingCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardBackNavigationShouldShowSafeBrowsingCard', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    setSyncEnabled(true);
    setSafeBrowsingSetting(SafeBrowsingSetting.STANDARD);
    assertCookiesCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertSafeBrowsingCardVisible();
  });

  test('cookiesCardBackNavigationShouldHideSafeBrowsingCard', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    setSyncEnabled(true);
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    assertCookiesCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('cookiesCardForwardNavigation', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    assertCookiesCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardGetsUpdated', function() {
    navigateToStep(PrivacyReviewStep.COOKIES);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertCookiesCardVisible();
    const radioButtonGroup =
        page.shadowRoot.querySelector('#cookiesFragment')
            .shadowRoot.querySelector('#cookiesRadioGroup');
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

  test('completionCardBackToSettingsNavigation', function() {
    navigateToStep(PrivacyReviewStep.COMPLETION);
    assertCompletionCardVisible();

    return whenPopState(function() {
             const completionFragment =
                 page.shadowRoot.querySelector('#completionFragment');
             completionFragment.shadowRoot.querySelector('#leaveButton')
                 .click();
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
        page.shadowRoot.querySelector('#completionFragment');
    assertTrue(isChildVisible(completionFragment, '#privacySandboxRow'));
    assertTrue(isChildVisible(completionFragment, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(completionFragment, '#privacySandboxRow'));
    assertFalse(isChildVisible(completionFragment, '#waaRow'));
  });
});

suite('HistorySyncFragment', function() {
  /** @type {!PrivacyReviewHistorySyncFragmentElement} */
  let page;
  /** @type {!SyncBrowserProxy} */
  let syncBrowserProxy;

  setup(function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = '';
    page = /** @type {!PrivacyReviewHistorySyncFragmentElement} */
        (document.createElement('privacy-review-history-sync-fragment'));
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  /**
   * @param {!{
   *   syncAllDataTypes: boolean,
   *   typedUrlsSynced: boolean,
   *   passwordsSynced: boolean,
   * }} destructured1
   */
  function setSyncStatus({
    syncAllDataTypes,
    typedUrlsSynced,
    passwordsSynced,
  }) {
    if (syncAllDataTypes) {
      assertTrue(typedUrlsSynced);
      assertTrue(passwordsSynced);
    }
    const event = {};
    for (const datatype of syncPrefsIndividualDataTypes) {
      event[datatype] = true;
    }
    // Overwrite datatypes needed in tests.
    event.syncAllDataTypes = syncAllDataTypes;
    event.typedUrlsSynced = typedUrlsSynced;
    event.passwordsSynced = passwordsSynced;
    webUIListenerCallback('sync-prefs-changed', event);
    flush();
  }

  /**
   * @param {!{
   *   syncAllDatatypesExpected: boolean,
   *   typedUrlsSyncedExpected: boolean,
   * }} destructured1
   */
  async function assertBrowserProxyCall({
    syncAllDatatypesExpected,
    typedUrlsSyncedExpected,
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
    page.shadowRoot.querySelector('#historyToggle').click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync re-enables sync all if sync all was on before
    // and if all sync datatypes are still enabled.
    page.shadowRoot.querySelector('#historyToggle').click();
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
    page.shadowRoot.querySelector('#historyToggle').click();
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
    page.shadowRoot.querySelector('#historyToggle').click();
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
    page.shadowRoot.querySelector('#historyToggle').click();
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
    page.shadowRoot.querySelector('#historyToggle').click();
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
    page.shadowRoot.querySelector('#historyToggle').click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync doesn't re-enable sync all if sync all wasn't on
    // originally.
    page.shadowRoot.querySelector('#historyToggle').click();
    return assertBrowserProxyCall({
      syncAllDataTypes: false,
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffEnableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: true,
    });
    page.shadowRoot.querySelector('#historyToggle').click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });
});
