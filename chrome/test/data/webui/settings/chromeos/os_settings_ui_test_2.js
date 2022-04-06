// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting, setUserActionRecorderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {eventToPromise, waitBeforeNextRender} from '../../../test_util.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';

import {FakeUserActionRecorder} from './fake_user_action_recorder.js';

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
  /** @type {!FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings = null;

  setup(async () => {
    PolymerTest.clearBody();

    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);

    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    await CrSettingsPrefs.initialized;
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);
    ui.$$('#drawerTemplate').if = false;
    flush();
  });

  teardown(() => {
    ui.remove();
    setUserActionRecorderForTesting(null);
    Router.getInstance().resetRouteForTesting();
  });

  test('top container shadow always shows for sub-pages', () => {
    const element = ui.$$('#cr-container-shadow-top');
    assertTrue(!!element, 'Shadow container element always exists');

    assertFalse(
        element.classList.contains('has-shadow'),
        'Main page should not show shadow ' + element.className);

    Router.getInstance().navigateTo(routes.POWER);
    flush();
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
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

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
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

    ui.isNarrow = false;
    flush();
    await eventToPromise('close', drawer);
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
    flush();

    assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
    assertTrue(ui.advancedOpenedInMain_);
    assertTrue(ui.advancedOpenedInMenu_);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(main.advancedToggleExpanded);

    ui.$$('#drawerTemplate').if = true;
    flush();

    const drawerMenu = ui.$$('cr-drawer os-settings-menu');
    assertTrue(!!drawerMenu);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(drawerMenu.advancedOpened);

    // Collapse 'Advanced' in the menu.
    drawerMenu.$.advancedButton.click();
    flush();

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
    flush();

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
    Router.getInstance().navigateTo(routes.BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.osPeople.click();
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });

  test('Clicking About menu item should focus About section', async () => {
    const router = Router.getInstance();
    const settingsMenu = ui.$$('os-settings-menu');

    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.$$('iron-selector');
    ironSelector.forceSynchronousItemUpdate();

    const {aboutItem} = settingsMenu.$;
    aboutItem.click();

    flush();
    assertEquals(routes.ABOUT_ABOUT, router.getCurrentRoute());
    assertNotEquals(aboutItem, settingsMenu.shadowRoot.activeElement);

    const settingsMain = ui.$$('os-settings-main');
    const aboutPage = settingsMain.$$('os-settings-about-page');
    await waitBeforeNextRender(aboutPage);
    const aboutSection = aboutPage.$$('settings-section[section="about"]');
    assertEquals(aboutSection, aboutPage.shadowRoot.activeElement);
  });

  test('userActionRouteChange', function() {
    assertEquals(userActionRecorder.navigationCount, 0);
    Router.getInstance().navigateTo(routes.BASIC);
    flush();
    assertEquals(userActionRecorder.navigationCount, 1);
    Router.getInstance().navigateTo(routes.BASIC);
    flush();
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
    ui.$$('#prefs').dispatchEvent(new CustomEvent(
        'user-action-setting-change',
        {bubbles: true, composed: true, detail: {}}));
    assertEquals(userActionRecorder.settingChangeCount, 1);
  });

  test('toolbar and nav menu are hidden in kiosk mode', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
    });

    ui.remove();
    Router.getInstance().resetRouteForTesting();
    PolymerTest.clearBody();
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    // Toolbar should be hidden.
    assertFalse(isVisible(ui.$$('os-toolbar')));
    // All navigation settings menus should be hidden.
    assertFalse(isVisible(ui.$$('os-settings-menu')));
  });
});
