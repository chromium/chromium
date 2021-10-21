// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyReviewHistorySyncFragmentElement, SettingsPrivacyReviewPageElement} from 'chrome://settings/lazy_load.js';
import {Route, Router, routes, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.js';

import {assertEquals, assertFalse} from '../chai_assert.js';
import {flushTasks, isChildVisible} from '../test_util.js';

// clang-format on

/* Maximum number of steps in the privacy review, excluding the welcome step. */
const PRIVACY_REVIEW_STEPS = 4;

suite('PrivacyReviewPage', function() {
  /** @type {!SettingsPrivacyReviewPageElement} */
  let page;
  let isSyncOn;
  let shouldShowCookiesCard;

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
      },
    };
    document.body.appendChild(page);
    isSyncOn = false;
    shouldShowCookiesCard = true;

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
   * @param {!{
   *   headerTextExpected: (string|undefined),
   *   isSettingFooterVisibleExpected: (boolean|undefined),
   *   isBackButtonVisibleExpected: (boolean|undefined),
   *   isWelcomeFragmentVisibleExpected: (boolean|undefined),
   *   isCompletionFragmentVisibleExpected: (boolean|undefined),
   *   isMsbbFragmentVisibleExpected: (boolean|undefined),
   *   isClearOnExitFragmentVisibleExpected: (boolean|undefined),
   *   isHistorySyncFragmentVisibleExpected: (boolean|undefined),
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
        !!isCookiesFragmentVisibleExpected,
        isChildVisible(page, '#cookiesFragment'));
  }

  /**
   * @return The expected total number of active cards for the step indicator.
   */
  function getExpectedNumberOfActiveCards() {
    return PRIVACY_REVIEW_STEPS - (isSyncOn ? 0 : 1) -
        (shouldShowCookiesCard ? 0 : 1);
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
    assertQueryParameter('welcome');
    assertCardComponentsVisible({
      isWelcomeFragmentVisibleExpected: true,
    });
  }

  function assertCompletionCardVisible() {
    assertQueryParameter('completion');
    assertCardComponentsVisible({
      isCompletionFragmentVisibleExpected: true,
    });
  }

  function assertMsbbCardVisible() {
    assertQueryParameter('msbb');
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewMsbbCardHeader'),
      isSettingFooterVisibleExpected: true,
      isMsbbFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(0);
  }

  function assertClearOnExitCardVisible() {
    assertQueryParameter('clearOnExit');
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewClearOnExitCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isClearOnExitFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(1);
  }

  function assertHistorySyncCardVisible() {
    assertQueryParameter('historySync');
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewHistorySyncCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isHistorySyncFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(1);
  }

  function assertCookiesCardVisible() {
    assertQueryParameter('cookies');
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewCookiesCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isCookiesFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(isSyncOn ? 2 : 1);
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
    navigateToStep('msbb');
    setSyncEnabled(true);
    assertMsbbCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('msbbForwardNavigationSyncOff', function() {
    navigateToStep('msbb');
    setSyncEnabled(false);
    assertMsbbCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCookiesCardVisible();
  });

  test('historySyncBackNavigation', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertMsbbCardVisible();
  });

  test('historySyncNavigatesAwayOnSyncOff', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    // User disables sync while history sync card is shown.
    setSyncEnabled(false);
    assertCookiesCardVisible();
  });

  test('historySyncNotReachableWhenSyncOff', function() {
    navigateToStep('historySync');
    setSyncEnabled(false);
    assertCookiesCardVisible();
  });

  test('historySyncCardForwardNavigationShouldShowCookiesCard', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertHistorySyncCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCookiesCardVisible();
  });

  test('historySyncCardForwardNavigationShouldHideCookiesCard', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    setCookieSetting(CookiePrimarySetting.ALLOW_ALL);
    assertHistorySyncCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardBackNavigationSyncOn', function() {
    navigateToStep('cookies');
    setSyncEnabled(true);
    assertCookiesCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('cookiesCardBackNavigationSyncOff', function() {
    navigateToStep('cookies');
    setSyncEnabled(false);
    assertCookiesCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertMsbbCardVisible();
  });

  test('cookiesCardForwardNavigation', function() {
    navigateToStep('cookies');
    assertCookiesCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardGetsUpdated', function() {
    navigateToStep('cookies');
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertCookiesCardVisible();
    const radioButtonGroup =
        page.shadowRoot.querySelector('#cookiesFragment')
            .shadowRoot.querySelector('#primarySettingGroup');
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
    navigateToStep('completion');
    assertCompletionCardVisible();

    return whenPopState(function() {
             page.shadowRoot.querySelector('#completeButton').click();
           })
        .then(function() {
          assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
        });
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
   * @param {boolean} syncAllOnBefore If sync-all is on before the click.
   * @param {boolean} historySyncOnBefore If history sync is on before the
   *     click.
   * @param {boolean} historySyncOnAfter If history sync is expected to be on
   *     after the click.
   */
  async function assertBrowserProxyCallOnToggleClicked(
      syncAllOnBefore, historySyncOnBefore, historySyncOnAfterwardsExpected) {
    const event = {
      syncAllDataTypes: syncAllOnBefore,
      typedUrlsSynced: historySyncOnBefore,
    };
    webUIListenerCallback('sync-prefs-changed', event);
    flush();

    page.shadowRoot.querySelector('#historyToggle').click();

    const syncPrefs = await syncBrowserProxy.whenCalled('setSyncDatatypes');
    assertFalse(syncPrefs.syncAllDataTypes);
    assertEquals(historySyncOnAfterwardsExpected, syncPrefs.typedUrlsSynced);
  }

  test('syncAllOnDisableHistorySync', async function() {
    assertBrowserProxyCallOnToggleClicked(
        /*syncAllOnBefore=*/ true, /*historySyncOnBefore=*/ true,
        /*historySyncOnAfterwardsExpected=*/ false);
  });

  test('syncAllOffDisableHistorySync', async function() {
    assertBrowserProxyCallOnToggleClicked(
        /*syncAllOnBefore=*/ false, /*historySyncOnBefore=*/ true,
        /*historySyncOnAfterwardsExpected=*/ false);
  });

  test('syncAllOffEnableHistorySync', async function() {
    assertBrowserProxyCallOnToggleClicked(
        /*syncAllOnBefore=*/ false, /*historySyncOnBefore=*/ false,
        /*historySyncOnAfterwardsExpected=*/ true);
  });
});
