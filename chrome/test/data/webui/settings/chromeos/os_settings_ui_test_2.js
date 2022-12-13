// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting, setUserActionRecorderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

import {FakeUserActionRecorder} from './fake_user_action_recorder.js';

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
    ui.shadowRoot.querySelector('#drawerTemplate').if = false;
    flush();
  });

  teardown(() => {
    ui.remove();
    setUserActionRecorderForTesting(null);
    Router.getInstance().resetRouteForTesting();
  });

  test('top container shadow always shows for sub-pages', () => {
    const element = ui.shadowRoot.querySelector('#cr-container-shadow-top');
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
    const toolbar = assert(ui.shadowRoot.querySelector('os-toolbar'));
    ui.isNarrow = true;
    assertTrue(toolbar.showMenu);

    ui.isNarrow = false;
    assertFalse(toolbar.showMenu);
  });

  test('app drawer', async () => {
    assertEquals(
        null, ui.shadowRoot.querySelector('cr-drawer os-settings-menu'));
    const drawer = ui.shadowRoot.querySelector('#drawer');
    assertFalse(drawer.open);

    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    assertTrue(!!ui.shadowRoot.querySelector('cr-drawer os-settings-menu'));

    drawer.cancel();
    // Drawer is closed, but menu is still stamped so its contents remain
    // visible as the drawer slides out.
    assertTrue(!!ui.shadowRoot.querySelector('cr-drawer os-settings-menu'));
  });

  test('app drawer closes when exiting narrow mode', async () => {
    const drawer = ui.shadowRoot.querySelector('#drawer');
    const toolbar = ui.shadowRoot.querySelector('os-toolbar');

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
    const main = ui.shadowRoot.querySelector('os-settings-main');
    const floatingMenu = ui.shadowRoot.querySelector('#left os-settings-menu');
    assertTrue(!!main);
    assertTrue(!!floatingMenu);

    assertFalse(!!ui.shadowRoot.querySelector('cr-drawer os-settings-menu'));
    assertFalse(ui.advancedOpenedInMain_);
    assertFalse(ui.advancedOpenedInMenu_);
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(main.advancedToggleExpanded);

    main.advancedToggleExpanded = true;
    flush();

    assertFalse(!!ui.shadowRoot.querySelector('cr-drawer os-settings-menu'));
    assertTrue(ui.advancedOpenedInMain_);
    assertTrue(ui.advancedOpenedInMenu_);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(main.advancedToggleExpanded);

    ui.shadowRoot.querySelector('#drawerTemplate').if = true;
    flush();

    const drawerMenu =
        ui.shadowRoot.querySelector('cr-drawer os-settings-menu');
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
    const settingsMenu = ui.shadowRoot.querySelector('os-settings-menu');

    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.shadowRoot.querySelector('iron-selector');
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
    const settingsMenu = ui.shadowRoot.querySelector('os-settings-menu');

    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.shadowRoot.querySelector('iron-selector');
    ironSelector.forceSynchronousItemUpdate();

    const {aboutItem} = settingsMenu.$;
    aboutItem.click();

    flush();
    assertEquals(routes.ABOUT_ABOUT, router.getCurrentRoute());
    assertNotEquals(aboutItem, settingsMenu.shadowRoot.activeElement);

    const settingsMain = ui.shadowRoot.querySelector('os-settings-main');
    const aboutPage =
        settingsMain.shadowRoot.querySelector('os-settings-about-page');
    await waitBeforeNextRender(aboutPage);
    const aboutSection =
        aboutPage.shadowRoot.querySelector(
            'os-settings-section[section="about"]');
    assertEquals(aboutSection, aboutPage.shadowRoot.activeElement);
  });

  test(
      'Detailed build info page is directly navigable and renders',
      async () => {
        const router = Router.getInstance();
        router.navigateTo(routes.DETAILED_BUILD_INFO);

        const settingsMain = ui.shadowRoot.querySelector('os-settings-main');
        const aboutPage =
            settingsMain.shadowRoot.querySelector('os-settings-about-page');
        const detailedBuildInfoPage =
            aboutPage.shadowRoot.querySelector('settings-detailed-build-info');
        await waitBeforeNextRender(detailedBuildInfoPage);
        assertTrue(isVisible(detailedBuildInfoPage));
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
    ui.shadowRoot.querySelector('#prefs').dispatchEvent(new CustomEvent(
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
    assertFalse(isVisible(ui.shadowRoot.querySelector('os-toolbar')));
    // All navigation settings menus should be hidden.
    assertFalse(isVisible(ui.shadowRoot.querySelector('os-settings-menu')));
  });
});
