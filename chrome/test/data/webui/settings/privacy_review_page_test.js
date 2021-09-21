// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsPrivacyReviewPageElement} from 'chrome://settings/lazy_load.js';
import {Route, Router, routes} from 'chrome://settings/settings.js';

import {assertEquals} from '../chai_assert.js';
import {flushTasks, isChildVisible} from '../test_util.js';

// clang-format on

suite('PrivacyReviewPage', function() {
  /** @type {!SettingsPrivacyReviewPageElement} */
  let page;

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyReviewPageElement} */
        (document.createElement('settings-privacy-review-page'));
    document.body.appendChild(page);

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
  }

  function assertClearOnExitCardVisible() {
    assertQueryParameter('clearOnExit');
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewClearOnExitCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isClearOnExitFragmentVisibleExpected: true,
    });
  }

  function assertHistorySyncCardVisible() {
    assertQueryParameter('historySync');
    assertCardComponentsVisible({
      headerTextExpected: page.i18n('privacyReviewHistorySyncCardHeader'),
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isHistorySyncFragmentVisibleExpected: true,
    });
  }

  test('welcomeForwardNavigation', function() {
    // Navigating to the privacy review without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
    assertWelcomeCardVisible();

    page.shadowRoot.querySelector('#startButton').click();
    flush();
    assertMsbbCardVisible();
  });

  test('msbbForwardNavigation', function() {
    navigateToStep('msbb');
    assertMsbbCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertClearOnExitCardVisible();
  });

  test('clearOnExitBackNavigation', function() {
    navigateToStep('clearOnExit');
    assertClearOnExitCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertMsbbCardVisible();
  });

  test('clearOnExitForwardNavigationSyncOn', function() {
    navigateToStep('clearOnExit');
    setSyncEnabled(true);
    assertClearOnExitCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('clearOnExitForwardNavigationSyncOff', function() {
    navigateToStep('clearOnExit');
    setSyncEnabled(false);
    assertClearOnExitCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });

  test('historySyncBackNavigation', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    page.shadowRoot.querySelector('#backButton').click();
    flush();
    assertClearOnExitCardVisible();
  });

  test('historySyncCardForwardNavigation', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });

  test('historySyncNavigatesAwayOnSyncOff', function() {
    navigateToStep('historySync');
    setSyncEnabled(true);
    assertHistorySyncCardVisible();

    // User disables sync while history sync card is shown.
    setSyncEnabled(false);
    assertCompletionCardVisible();
  });

  test('historySyncNotReachableWhenSyncOff', function() {
    navigateToStep('historySync');
    setSyncEnabled(false);
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
