// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {osPageVisibility, Route, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @fileoverview Runs tests for the OS settings menu. */

function setupRouter() {
  const testRoutes = {
    BASIC: new Route('/'),
    ABOUT: new Route('/about'),
    ADVANCED: new Route('/advanced'),
  };
  testRoutes.BLUETOOTH =
      testRoutes.BASIC.createSection('/bluetooth', 'bluetooth');
  testRoutes.RESET = testRoutes.ADVANCED.createSection('/osReset', 'osReset');

  Router.resetInstanceForTesting(new Router(testRoutes));

  routes.RESET = testRoutes.RESET;
  routes.BLUETOOTH = testRoutes.BLUETOOTH;
  routes.ADVANCED = testRoutes.ADVANCED;
  routes.BASIC = testRoutes.BASIC;
}

suite('OSSettingsMenu', function() {
  let settingsMenu = null;

  setup(function() {
    setupRouter();
    PolymerTest.clearBody();
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageVisibility = osPageVisibility;
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('advancedOpenedBinding', function() {
    assertFalse(settingsMenu.advancedOpened);
    settingsMenu.advancedOpened = true;
    flush();
    assertTrue(settingsMenu.isAdvancedSubmenuOpenedForTest());

    settingsMenu.advancedOpened = false;
    flush();
    assertFalse(settingsMenu.isAdvancedSubmenuOpenedForTest());
  });

  test('tapAdvanced', function() {
    assertFalse(settingsMenu.advancedOpened);

    const advancedToggle =
        settingsMenu.shadowRoot.querySelector('#advancedButton');
    assertTrue(!!advancedToggle);

    advancedToggle.click();
    flush();
    assertTrue(settingsMenu.isAdvancedSubmenuOpenedForTest());

    advancedToggle.click();
    flush();
    assertFalse(settingsMenu.isAdvancedSubmenuOpenedForTest());
  });

  test('upAndDownIcons', function() {
    // There should be different icons for a top level menu being open
    // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
    const ironIconElement =
        settingsMenu.shadowRoot.querySelector('#advancedButton iron-icon');
    assertTrue(!!ironIconElement);

    settingsMenu.advancedOpened = true;
    flush();
    const openIcon = ironIconElement.icon;
    assertTrue(!!openIcon);

    settingsMenu.advancedOpened = false;
    flush();
    assertNotEquals(openIcon, ironIconElement.icon);
  });

  test('Advanced menu expands on navigating to an advanced setting', () => {
    assertFalse(settingsMenu.advancedOpened);
    Router.getInstance().navigateTo(routes.RESET);
    assertFalse(settingsMenu.advancedOpened);

    // If there are search params and the current route is a descendant of
    // the Advanced route, then ensure that the advanced menu expands.
    const params = new URLSearchParams('search=test');
    Router.getInstance().navigateTo(routes.RESET, params);
    flush();
    assertTrue(settingsMenu.advancedOpened);
  });
});

suite('OSSettingsMenuReset', function() {
  let settingsMenu = null;

  setup(function() {
    setupRouter();
    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.RESET, '');
    settingsMenu = document.createElement('os-settings-menu');
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('openResetSection', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/osReset', path);
  });

  test('navigateToAnotherSection', function() {
    const selector = settingsMenu.$.subMenu;
    let path = new window.URL(selector.selected).pathname;
    assertEquals('/osReset', path);

    Router.getInstance().navigateTo(routes.BLUETOOTH, '');
    flush();

    path = new window.URL(selector.selected).pathname;
    assertEquals('/bluetooth', path);
  });

  test('navigateToBasic', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/osReset', path);

    Router.getInstance().navigateTo(routes.BASIC, '');
    flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });
});
