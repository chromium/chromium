// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {Route, Router} from 'chrome://settings/settings.js';
import {setupPopstateListener} from 'chrome://test/settings/test_util.js';
import {eventToPromise} from 'chrome://test/test_util.js';

// clang-format on

suite('settings-animated-pages', function() {
  /** @type {?SettingsRoutes}  */
  let testRoutes = null;

  setup(function() {
    testRoutes = {
      BASIC: new Route('/'),
    };
    testRoutes.SEARCH = testRoutes.BASIC.createSection('/search', 'search');
    testRoutes.SEARCH_ENGINES = testRoutes.SEARCH.createChild('/searchEngines');

    testRoutes.PRIVACY = testRoutes.BASIC.createSection('/privacy', 'privacy');
    testRoutes.SITE_SETTINGS = testRoutes.PRIVACY.createChild('/content');
    testRoutes.SITE_SETTINGS_COOKIES =
        testRoutes.PRIVACY.createChild('/cookies');

    Router.resetInstanceForTesting(new Router(testRoutes));
    setupPopstateListener();
  });

  // Test simple case where the |focusConfig| key captures only the previous
  // route.
  test('FocusSubpageTrigger_SimpleKey', async function() {
    document.body.innerHTML = `
      <settings-animated-pages section="${testRoutes.SEARCH_ENGINES.section}">
        <div route-path="default">
          <button id="subpage-trigger"></button>
        </div>
        <div route-path="${testRoutes.SEARCH_ENGINES.path}"></div>
      </settings-animated-pages>`;

    const animatedPages =
        document.body.querySelector('settings-animated-pages');
    animatedPages.focusConfig = new Map();
    animatedPages.focusConfig.set(
        testRoutes.SEARCH_ENGINES.path, '#subpage-trigger');

    const trigger = document.body.querySelector('#subpage-trigger');
    assertTrue(!!trigger);
    const whenDone = eventToPromise('focus', trigger);

    // Trigger subpage exit navigation.
    Router.getInstance().navigateTo(testRoutes.BASIC);
    Router.getInstance().navigateTo(testRoutes.SEARCH_ENGINES);
    Router.getInstance().navigateToPreviousRoute();
    await whenDone;
  });

  // Test case where the |focusConfig| key captures both previous and current
  // route, to differentiate cases where a subpage can have multiple entry
  // points.
  test('FocusSubpageTrigger_FromToKey', async function() {
    document.body.innerHTML = `
      <settings-animated-pages section="${testRoutes.PRIVACY.section}">
        <div route-path="default">
          <button id="subpage-trigger1"></button>
        </div>
        <div route-path="${testRoutes.SITE_SETTINGS.path}">
          <button id="subpage-trigger2"></button>
        </div>
        <div route-path="${testRoutes.SITE_SETTINGS_COOKIES.path}"></div>
      </settings-animated-pages>`;

    const animatedPages =
        document.body.querySelector('settings-animated-pages');
    animatedPages.focusConfig = new Map();
    animatedPages.focusConfig.set(
        testRoutes.SITE_SETTINGS_COOKIES.path + '_' + testRoutes.PRIVACY.path,
        '#subpage-trigger1');
    animatedPages.focusConfig.set(
        testRoutes.SITE_SETTINGS_COOKIES.path + '_' +
            testRoutes.SITE_SETTINGS.path,
        '#subpage-trigger2');

    // Trigger subpage back navigation from entry point A, and check that the
    // correct element #subpageTrigger1 is focused.
    const trigger1 = document.body.querySelector('#subpage-trigger1');
    assertTrue(!!trigger1);
    let whenDone = eventToPromise('focus', trigger1);

    Router.getInstance().navigateTo(testRoutes.PRIVACY);
    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS_COOKIES);
    Router.getInstance().navigateToPreviousRoute();
    await whenDone;

    // Trigger subpage back navigation from entry point B, and check that the
    // correct element #subpageTrigger1 is focused.
    const trigger2 = document.body.querySelector('#subpage-trigger2');
    assertTrue(!!trigger2);
    whenDone = eventToPromise('focus', trigger2);

    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS);
    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS_COOKIES);
    Router.getInstance().navigateToPreviousRoute();
    await whenDone;
  });

  test('IgnoresBubblingIronSelect', async function() {
    document.body.innerHTML = `
      <settings-animated-pages section="${testRoutes.PRIVACY.section}">
        <div route-path="default"></div>
        <settings-subpage route-path="${testRoutes.SITE_SETTINGS.path}">
          <div></div>
        </settings-subpage>
      </settings-animated-pages>`;

    const subpage = document.body.querySelector('settings-subpage');
    let counter = 0;

    const whenFired = new Promise(resolve => {
      // Override |focusBackButton| to check how many times it is called.
      subpage.focusBackButton = () => {
        counter++;

        if (counter === 1) {
          const other = document.body.querySelector('div');
          other.dispatchEvent(new CustomEvent('iron-select', {bubbles: true}));
          resolve();
        }
      };
    });

    Router.getInstance().navigateTo(testRoutes.PRIVACY);
    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS);

    await whenFired;

    // Ensure that |focusBackButton| was only called once, ignoring the
    // any unrelated 'iron-select' events.
    assertEquals(1, counter);
  });
});
