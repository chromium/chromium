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
   *   headerTextExpected: (string|undefined),
   *   isSettingFooterVisibleExpected: (boolean|undefined),
   *   isWelcomeFragmentVisibleExpected: (boolean|undefined),
   *   isCompletionFragmentVisibleExpected: (boolean|undefined),
   *   isMsbbFragmentVisibleExpected: (boolean|undefined),
   *   isClearOnExitFragmentVisibleExpected: (boolean|undefined),
   * }} destructured1
   */
  function assertCardVisibility({
    headerTextExpected,
    isSettingFooterVisibleExpected,
    isWelcomeFragmentVisibleExpected,
    isCompletionFragmentVisibleExpected,
    isMsbbFragmentVisibleExpected,
    isClearOnExitFragmentVisibleExpected,
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
  }

  function assertWelcomeCardVisible() {
    assertQueryParameter('welcome');
    assertCardVisibility({
      isWelcomeFragmentVisibleExpected: true,
    });
  }

  function assertCompletionCardVisible() {
    assertQueryParameter('completion');
    assertCardVisibility({
      isCompletionFragmentVisibleExpected: true,
    });
  }

  function assertMsbbCardVisible() {
    assertQueryParameter('msbb');
    assertCardVisibility({
      headerTextExpected: page.i18n('privacyReviewMsbbCardHeader'),
      isSettingFooterVisibleExpected: true,
      isMsbbFragmentVisibleExpected: true,
    });
  }

  function assertClearOnExitCardVisible() {
    assertQueryParameter('clearOnExit');
    assertCardVisibility({
      headerTextExpected: page.i18n('privacyReviewClearOnExitCardHeader'),
      isSettingFooterVisibleExpected: true,
      isClearOnExitFragmentVisibleExpected: true,
    });
  }

  test('testForwardNavigation', function() {
    // The review starts with the welcome card.
    assertWelcomeCardVisible();

    // Advance to the MSBB card.
    page.shadowRoot.querySelector('#startButton').click();
    flush();
    assertMsbbCardVisible();

    // Advance to the clear on exit card.
    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertClearOnExitCardVisible();

    // Advance to the completion card.
    page.shadowRoot.querySelector('#nextButton').click();
    flush();
    assertCompletionCardVisible();
  });
});
