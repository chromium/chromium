// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {pageVisibility, Router, routes} from 'chrome://settings/settings.js';
// clang-format on

suite('SettingsMenu', function() {
  let settingsMenu = null;

  setup(function() {
    PolymerTest.clearBody();
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = pageVisibility;
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('advancedOpenedBinding', function() {
    assertFalse(settingsMenu.advancedOpened);
    settingsMenu.advancedOpened = true;
    flush();
    assertTrue(settingsMenu.$.advancedSubmenu.opened);

    settingsMenu.advancedOpened = false;
    flush();
    assertFalse(settingsMenu.$.advancedSubmenu.opened);
  });

  test('tapAdvanced', function() {
    assertFalse(settingsMenu.advancedOpened);

    const advancedToggle = settingsMenu.$$('#advancedButton');
    assertTrue(!!advancedToggle);

    advancedToggle.click();
    flush();
    assertTrue(settingsMenu.$.advancedSubmenu.opened);

    advancedToggle.click();
    flush();
    assertFalse(settingsMenu.$.advancedSubmenu.opened);
  });

  test('upAndDownIcons', function() {
    // There should be different icons for a top level menu being open
    // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
    const ironIconElement = settingsMenu.$$('#advancedButton iron-icon');
    assertTrue(!!ironIconElement);

    settingsMenu.advancedOpened = true;
    flush();
    const openIcon = ironIconElement.icon;
    assertTrue(!!openIcon);

    settingsMenu.advancedOpened = false;
    flush();
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
    Router.getInstance().navigateTo(routes.BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.people.click();
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });
});

suite('SettingsMenuReset', function() {
  let settingsMenu = null;

  setup(function() {
    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.RESET, '');
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    flush();
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

    Router.getInstance().navigateTo(routes.PEOPLE, '');
    flush();

    path = new window.URL(selector.selected).pathname;
    assertEquals('/people', path);
  });

  test('navigateToBasic', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);

    Router.getInstance().navigateTo(routes.BASIC, '');
    flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });

  test('pageVisibility', function() {
    function assertPagesHidden(expectedHidden) {
      assertEquals(expectedHidden, settingsMenu.$$('#people').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#appearance').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#onStartup').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#advancedButton').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#advancedSubmenu').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#reset').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#safetyCheck').hidden);

      if (!isChromeOS && !isLacros) {
        assertEquals(expectedHidden, settingsMenu.$$('#defaultBrowser').hidden);
      }
    }

    // The default pageVisibility should not cause menu items to be hidden.
    assertPagesHidden(false);

    // Set the visibility of the pages under test to "false".
    settingsMenu.pageVisibility = Object.assign(pageVisibility || {}, {
      advancedSettings: false,
      appearance: false,
      defaultBrowser: false,
      multidevice: false,
      onStartup: false,
      people: false,
      reset: false,
      safetyCheck: false,
    });
    flush();

    // Now, the menu items should be hidden.
    assertPagesHidden(true);
  });
});
