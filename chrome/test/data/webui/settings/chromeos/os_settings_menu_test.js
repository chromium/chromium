// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {Router, routes, Route, pageVisibility} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

/** @fileoverview Runs tests for the OS settings menu. */

function setupRouter() {
  const testRoutes = {
    BASIC: new settings.Route('/'),
    ADVANCED: new settings.Route('/advanced'),
  };
  testRoutes.BLUETOOTH =
      testRoutes.BASIC.createSection('/bluetooth', 'bluetooth');
  testRoutes.RESET = testRoutes.ADVANCED.createSection('/osReset', 'osReset');

  settings.Router.resetInstanceForTesting(new settings.Router(testRoutes));

  settings.routes.RESET = testRoutes.RESET;
  settings.routes.BLUETOOTH = testRoutes.BLUETOOTH;
  settings.routes.ADVANCED = testRoutes.ADVANCED;
  settings.routes.BASIC = testRoutes.BASIC;
}

suite('OSSettingsMenu', function() {
  let settingsMenu = null;

  setup(function() {
    setupRouter();
    PolymerTest.clearBody();
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageVisibility = settings.pageVisibility;
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('advancedOpenedBinding', function() {
    assertFalse(settingsMenu.advancedOpened);
    settingsMenu.advancedOpened = true;
    Polymer.dom.flush();
    assertTrue(settingsMenu.isAdvancedSubmenuOpenedForTest());

    settingsMenu.advancedOpened = false;
    Polymer.dom.flush();
    assertFalse(settingsMenu.isAdvancedSubmenuOpenedForTest());
  });

  test('tapAdvanced', function() {
    assertFalse(settingsMenu.advancedOpened);

    const advancedToggle = settingsMenu.$$('#advancedButton');
    assertTrue(!!advancedToggle);

    advancedToggle.click();
    Polymer.dom.flush();
    assertTrue(settingsMenu.isAdvancedSubmenuOpenedForTest());

    advancedToggle.click();
    Polymer.dom.flush();
    assertFalse(settingsMenu.isAdvancedSubmenuOpenedForTest());
  });

  test('upAndDownIcons', function() {
    // There should be different icons for a top level menu being open
    // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
    const ironIconElement = settingsMenu.$$('#advancedButton iron-icon');
    assertTrue(!!ironIconElement);

    settingsMenu.advancedOpened = true;
    Polymer.dom.flush();
    const openIcon = ironIconElement.icon;
    assertTrue(!!openIcon);

    settingsMenu.advancedOpened = false;
    Polymer.dom.flush();
    assertNotEquals(openIcon, ironIconElement.icon);
  });

  test('Advanced menu expands on navigating to an advanced setting', () => {
    assertFalse(settingsMenu.advancedOpened);
    settings.Router.getInstance().navigateTo(settings.routes.RESET);
    assertFalse(settingsMenu.advancedOpened);

    // If there are search params and the current route is a descendant of
    // the Advanced route, then ensure that the advanced menu expands.
    const params = new URLSearchParams('search=test');
    settings.Router.getInstance().navigateTo(settings.routes.RESET, params);
    Polymer.dom.flush();
    assertTrue(settingsMenu.advancedOpened);
  });
});

suite('OSSettingsMenuReset', function() {
  let settingsMenu = null;

  setup(function() {
    setupRouter();
    PolymerTest.clearBody();
    settings.Router.getInstance().navigateTo(settings.routes.RESET, '');
    settingsMenu = document.createElement('os-settings-menu');
    document.body.appendChild(settingsMenu);
    Polymer.dom.flush();
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

    settings.Router.getInstance().navigateTo(settings.routes.BLUETOOTH, '');
    Polymer.dom.flush();

    path = new window.URL(selector.selected).pathname;
    assertEquals('/bluetooth', path);
  });

  test('navigateToBasic', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/osReset', path);

    settings.Router.getInstance().navigateTo(settings.routes.BASIC, '');
    Polymer.dom.flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });
});
