// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the OS settings menu. */

suite('OSSettingsMenu', function() {
  let settingsMenu = null;

  setup(function() {
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

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', function() {
    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.$$('iron-selector');
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    settings.navigateTo(settings.routes.BASIC, urlParams);
    assertEquals(
        urlParams.toString(), settings.getQueryParameters().toString());
    settingsMenu.$.people.click();
    assertEquals('', settings.getQueryParameters().toString());
  });
});

suite('OSSettingsMenuReset', function() {
  setup(function() {
    PolymerTest.clearBody();
    settings.navigateTo(settings.routes.RESET, '');
    settingsMenu = document.createElement('os-settings-menu');
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('openResetSection', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);
  });

  test('navigateToAnotherSection', function() {
    const selector = settingsMenu.$.subMenu;
    let path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);

    settings.navigateTo(settings.routes.BLUETOOTH, '');
    Polymer.dom.flush();

    path = new window.URL(selector.selected).pathname;
    assertEquals('/bluetooth', path);
  });

  test('navigateToBasic', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);

    settings.navigateTo(settings.routes.BASIC, '');
    Polymer.dom.flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });
});
