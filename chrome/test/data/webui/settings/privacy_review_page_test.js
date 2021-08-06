// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
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
   *   isFooterHrVisibleExpected: (boolean|undefined),
   *   isWelcomeFragmentVisibleExpected: (boolean|undefined),
   *   isCompletionFragmentVisibleExpected: (boolean|undefined),
   *   isMsbbFragmentVisibleExpected: (boolean|undefined),
   * }} destructured1
   */
  function assertCardVisibility_({
    isHeaderVisibleExpected,
    isFooterVisibleExpected,
    isFooterHrVisibleExpected,
    isWelcomeFragmentVisibleExpected,
    isCompletionFragmentVisibleExpected,
    isMsbbFragmentVisibleExpected,
  }) {
    assertEquals(!!isHeaderVisibleExpected, isChildVisible(page, '#header'));
    assertEquals(!!isFooterVisibleExpected, isChildVisible(page, '#footer'));
    assertEquals(
        !!isFooterHrVisibleExpected,
        page.shadowRoot.querySelector('#footer').classList.contains('hr'));
    assertEquals(
        !!isWelcomeFragmentVisibleExpected,
        isChildVisible(page, '#welcomeFragment'));
    assertEquals(
        !!isCompletionFragmentVisibleExpected,
        isChildVisible(page, '#completionFragment'));
    assertEquals(
        !!isMsbbFragmentVisibleExpected, isChildVisible(page, '#msbbFragment'));
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
      isFooterHrVisibleExpected: true,
      isCompletionFragmentVisibleExpected: true,
    });
  }

  function assertMsbbCardVisible_() {
    assertQueryParameter('msbb');
    assertCardVisibility_({
      isHeaderVisibleExpected: true,
      isFooterVisibleExpected: true,
      isFooterHrVisibleExpected: true,
      isMsbbFragmentVisibleExpected: true,
    });
  }

  test('testForwardNavigation', function() {
    // The review starts with the welcome card.
    assertWelcomeCardVisible_();

    // Advance to the MSBB card.
    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertMsbbCardVisible_();

    // Advance to the completion card.
    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible_();
  });
});
