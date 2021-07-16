// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {SettingsPrivacyReviewPageElement} from 'chrome://settings/lazy_load.js';
import {Route, Router, routes} from 'chrome://settings/settings.js';

import {assertEquals} from '../chai_assert.js';
import {flushTasks, isChildVisible} from '../test_util.m.js';

// clang-format on

suite('PrivacyReviewPage', function() {
  /** @type {!SettingsPrivacyReviewPageElement} */
  let page;

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsPrivacyReviewPageElement} */
        (document.createElement('settings-privacy-review-page'));
    document.body.appendChild(page);

    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  /**
   * @param {string} step
   */
  function assertQueryParameter(step) {
    assertEquals(step, Router.getInstance().getQueryParameters().get('step'));
  }

  /**
   * @param {!{
   *   isHeaderVisibleExpected: ((boolean|undefined)|undefined),
   *   isFooterVisibleExpected: (boolean|undefined),
   *   isWelcomeFragmentVisibleExpected: (boolean|undefined),
   *   isCompletionFragmentVisibleExpected: (boolean|undefined),
   *   isCookiesFragmentVisibleExpected: (boolean|undefined),
   * }} destructured1
   */
  function assertCardVisibility_({
    isHeaderVisibleExpected,
    isFooterVisibleExpected,
    isWelcomeFragmentVisibleExpected,
    isCompletionFragmentVisibleExpected,
    isCookiesFragmentVisibleExpected,
  }) {
    assertEquals(!!isHeaderVisibleExpected, isChildVisible(page, '#header'));
    assertEquals(!!isFooterVisibleExpected, isChildVisible(page, '#footer'));
    assertEquals(
        !!isWelcomeFragmentVisibleExpected,
        isChildVisible(page, '#welcomeFragment'));
    assertEquals(
        !!isCompletionFragmentVisibleExpected,
        isChildVisible(page, '#completionFragment'));
    assertEquals(
        !!isCookiesFragmentVisibleExpected,
        isChildVisible(page, '#cookiesFragment'));
  }

  function assertWelcomeCardVisible_() {
    assertQueryParameter('welcome');
    assertCardVisibility_({
      isFooterVisibleExpected: true,
      isWelcomeFragmentVisibleExpected: true,
    });
  }

  function assertCompletionCardVisible_() {
    assertQueryParameter('completion');
    assertCardVisibility_({
      isFooterVisibleExpected: true,
      isCompletionFragmentVisibleExpected: true,
    });
  }

  function assertCookiesCardVisible_() {
    assertQueryParameter('cookies');
    assertCardVisibility_({
      isHeaderVisibleExpected: true,
      isFooterVisibleExpected: true,
      isCookiesFragmentVisibleExpected: true,
    });
  }

  test('testForwardNavigation', function() {
    // The review starts with the welcome card.
    assertWelcomeCardVisible_();

    // Advance to the cookies card.
    page.shadowRoot.querySelector('#nextButton').click();
    assertCookiesCardVisible_();

    // Advance to the completion card.
    page.shadowRoot.querySelector('#nextButton').click();
    assertCompletionCardVisible_();
  });
});
