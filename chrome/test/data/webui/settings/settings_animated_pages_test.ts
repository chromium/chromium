// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsRoutes} from 'chrome://settings/settings.js';
import {Route, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

import {setupPopstateListener} from './test_util.js';

// clang-format on

suite('settings-animated-pages', function() {
  let testRoutes: {
    ABOUT: Route,
    ADVANCED: Route,
    BASIC: Route,
    PRIVACY: Route,
    SEARCH_ENGINES: Route,
    SEARCH: Route,
    SITE_SETTINGS_COOKIES: Route,
    SITE_SETTINGS: Route,
  };

  setup(function() {
    const basicRoute = new Route('/');
    const searchRoute = basicRoute.createSection('/search', 'search');
    const privacyRoute = basicRoute.createSection('/privacy', 'privacy');

    testRoutes = {
      ABOUT: basicRoute.createChild('/about'),
      ADVANCED: basicRoute.createChild('/advanced'),
      BASIC: basicRoute,
      PRIVACY: privacyRoute,
      SEARCH_ENGINES: searchRoute.createChild('/searchEngines'),
      SEARCH: searchRoute,
      SITE_SETTINGS_COOKIES: privacyRoute.createChild('/cookies'),
      SITE_SETTINGS: privacyRoute.createChild('/content'),
    };

    Router.resetInstanceForTesting(new Router(testRoutes as SettingsRoutes));
    setupPopstateListener();
  });

  // Test simple case where the |focusConfig| key captures only the previous
  // route.
  test('FocusSubpageTrigger_SimpleKey', async function() {
    document.body.innerHTML = getTrustedHtml(`
      <settings-animated-pages section="${testRoutes.SEARCH_ENGINES.section}">
        <div route-path="default">
          <button id="subpage-trigger"></button>
        </div>
        <div route-path="${testRoutes.SEARCH_ENGINES.path}"></div>
      </settings-animated-pages>`);

    const animatedPages =
        document.body.querySelector('settings-animated-pages')!;
    animatedPages.focusConfig = new Map();
    animatedPages.focusConfig.set(
        testRoutes.SEARCH_ENGINES.path, '#subpage-trigger');

    const trigger = document.body.querySelector('#subpage-trigger');
    assertTrue(!!trigger);
    const whenDone = eventToPromise('focus', trigger!);

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
    document.body.innerHTML = getTrustedHtml(`
      <settings-animated-pages section="${testRoutes.PRIVACY.section}">
        <div route-path="default">
          <button id="subpage-trigger1"></button>
        </div>
        <div route-path="${testRoutes.SITE_SETTINGS.path}">
          <button id="subpage-trigger2"></button>
        </div>
        <div route-path="${testRoutes.SITE_SETTINGS_COOKIES.path}"></div>
      </settings-animated-pages>`);

    const animatedPages =
        document.body.querySelector('settings-animated-pages')!;
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
    let whenDone = eventToPromise('focus', trigger1!);

    Router.getInstance().navigateTo(testRoutes.PRIVACY);
    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS_COOKIES);
    Router.getInstance().navigateToPreviousRoute();
    await whenDone;

    // Trigger subpage back navigation from entry point B, and check that the
    // correct element #subpageTrigger1 is focused.
    const trigger2 = document.body.querySelector('#subpage-trigger2');
    assertTrue(!!trigger2);
    whenDone = eventToPromise('focus', trigger2!);

    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS);
    Router.getInstance().navigateTo(testRoutes.SITE_SETTINGS_COOKIES);
    Router.getInstance().navigateToPreviousRoute();
    await whenDone;
  });

  test('IgnoresBubblingIronSelect', async function() {
    document.body.innerHTML = getTrustedHtml(`
      <settings-animated-pages section="${testRoutes.PRIVACY.section}">
        <div route-path="default"></div>
        <settings-subpage route-path="${testRoutes.SITE_SETTINGS.path}">
          <div></div>
        </settings-subpage>
      </settings-animated-pages>`);

    const subpage = document.body.querySelector('settings-subpage')!;
    let counter = 0;

    const whenFired = new Promise<void>(resolve => {
      // Override |focusBackButton| to check how many times it is called.
      subpage.focusBackButton = () => {
        counter++;

        if (counter === 1) {
          const other = document.body.querySelector('div')!;
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
