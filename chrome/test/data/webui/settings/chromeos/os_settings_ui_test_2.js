// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {CrSettingsPrefs, Router, routes, setUserActionRecorderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeUserActionRecorder} from './fake_user_action_recorder.m.js';
// #import {eventToPromise} from '../../../test_util.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// clang-format on

/**
 * Checks whether a given element is visible to the user.
 * @param {!Element} element
 * @returns {boolean}
 */
function isVisible(element) {
  return !!(element && element.getBoundingClientRect().width > 0);
}

suite('os-settings-ui', () => {
  let ui;
  let userActionRecorder;

  setup(async () => {
    PolymerTest.clearBody();
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    await CrSettingsPrefs.initialized;
    userActionRecorder = new settings.FakeUserActionRecorder();
    settings.setUserActionRecorderForTesting(userActionRecorder);
    ui.$$('#drawerTemplate').if = false;
    Polymer.dom.flush();
  });

  teardown(() => {
    ui.remove();
    settings.setUserActionRecorderForTesting(null);
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('top container shadow always shows for sub-pages', () => {
    const element = ui.$$('#cr-container-shadow-top');
    assertTrue(!!element, 'Shadow container element always exists');

    assertFalse(
        element.classList.contains('has-shadow'),
        'Main page should not show shadow ' + element.className);

    settings.Router.getInstance().navigateTo(settings.routes.POWER);
    Polymer.dom.flush();
    assertTrue(
        element.classList.contains('has-shadow'),
        'Sub-page should show shadow ' + element.className);
  });

  test('showing menu in toolbar is dependent on narrow mode', () => {
    const toolbar = assert(ui.$$('os-toolbar'));
    ui.isNarrow = true;
    assertTrue(toolbar.showMenu);

    ui.isNarrow = false;
    assertFalse(toolbar.showMenu);
  });

  test('app drawer', async () => {
    assertEquals(null, ui.$$('cr-drawer os-settings-menu'));
    const drawer = ui.$$('#drawer');
    assertFalse(drawer.open);

    drawer.openDrawer();
    Polymer.dom.flush();
    await test_util.eventToPromise('cr-drawer-opened', drawer);

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    assertTrue(!!ui.$$('cr-drawer os-settings-menu'));

    drawer.cancel();
    // Drawer is closed, but menu is still stamped so its contents remain
    // visible as the drawer slides out.
    assertTrue(!!ui.$$('cr-drawer os-settings-menu'));
  });

  test('app drawer closes when exiting narrow mode', async () => {
    const drawer = ui.$$('#drawer');
    const toolbar = ui.$$('os-toolbar');

    // Mimic narrow mode and open the drawer.
    ui.isNarrow = true;
    drawer.openDrawer();
    Polymer.dom.flush();
    await test_util.eventToPromise('cr-drawer-opened', drawer);

    ui.isNarrow = false;
    Polymer.dom.flush();
    await test_util.eventToPromise('close', drawer);
    assertFalse(drawer.open);
  });

  test('advanced UIs stay in sync', () => {
    const main = ui.$$('os-settings-main');
    const floatingMenu = ui.$$('#left os-settings-menu');
    assertTrue(!!main);
    assertTrue(!!floatingMenu);

    assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
    assertFalse(ui.advancedOpenedInMain_);
    assertFalse(ui.advancedOpenedInMenu_);
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(main.advancedToggleExpanded);

    main.advancedToggleExpanded = true;
    Polymer.dom.flush();

    assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
    assertTrue(ui.advancedOpenedInMain_);
    assertTrue(ui.advancedOpenedInMenu_);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(main.advancedToggleExpanded);

    ui.$$('#drawerTemplate').if = true;
    Polymer.dom.flush();

    const drawerMenu = ui.$$('cr-drawer os-settings-menu');
    assertTrue(!!drawerMenu);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(drawerMenu.advancedOpened);

    // Collapse 'Advanced' in the menu.
    drawerMenu.$.advancedButton.click();
    Polymer.dom.flush();

    // Collapsing it in the menu should not collapse it in the main area.
    assertFalse(drawerMenu.advancedOpened);
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(ui.advancedOpenedInMenu_);
    assertTrue(main.advancedToggleExpanded);
    assertTrue(ui.advancedOpenedInMain_);

    // Expand both 'Advanced's again.
    drawerMenu.$.advancedButton.click();

    // Collapse 'Advanced' in the main area.
    main.advancedToggleExpanded = false;
    Polymer.dom.flush();

    // Collapsing it in the main area should not collapse it in the menu.
    assertFalse(ui.advancedOpenedInMain_);
    assertTrue(drawerMenu.advancedOpened);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(ui.advancedOpenedInMenu_);
  });

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', function() {
    const settingsMenu = ui.$$('os-settings-menu');

    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.$$('iron-selector');
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    settings.Router.getInstance().navigateTo(settings.routes.BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        settings.Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.osPeople.click();
    assertEquals(
        '', settings.Router.getInstance().getQueryParameters().toString());
  });

  test('userActionRouteChange', function() {
    assertEquals(userActionRecorder.navigationCount, 0);
    settings.Router.getInstance().navigateTo(settings.routes.BASIC);
    Polymer.dom.flush();
    assertEquals(userActionRecorder.navigationCount, 1);
    settings.Router.getInstance().navigateTo(settings.routes.BASIC);
    Polymer.dom.flush();
    assertEquals(userActionRecorder.navigationCount, 1);
  });

  test('userActionBlurEvent', function() {
    assertEquals(userActionRecorder.pageBlurCount, 0);
    ui.fire('blur');
    assertEquals(userActionRecorder.pageBlurCount, 1);
  });

  test('userActionClickEvent', () => {
    assertEquals(userActionRecorder.clickCount, 0);
    ui.fire('click');
    assertEquals(userActionRecorder.clickCount, 1);
  });

  test('userActionFocusEvent', function() {
    assertEquals(userActionRecorder.pageFocusCount, 0);
    ui.fire('focus');
    assertEquals(userActionRecorder.pageFocusCount, 1);
  });

  test('userActionPrefChange', function() {
    assertEquals(userActionRecorder.settingChangeCount, 0);
    ui.$$('#prefs').fire('user-action-setting-change');
    assertEquals(userActionRecorder.settingChangeCount, 1);
  });

  test('toolbar and nav menu are hidden in kiosk mode', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
    });

    ui.remove();
    settings.Router.getInstance().resetRouteForTesting();
    PolymerTest.clearBody();
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    Polymer.dom.flush();

    // Toolbar should be hidden.
    assertFalse(isVisible(ui.$$('os-toolbar')));
    // All navigation settings menus should be hidden.
    assertFalse(isVisible(ui.$$('os-settings-menu')));
  });
});
