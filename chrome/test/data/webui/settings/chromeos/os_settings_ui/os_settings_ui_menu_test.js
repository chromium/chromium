// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<os-settings-ui> menu', () => {
  let ui;
  let fakeNearbySettings;

  suiteSetup(() => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);
  });

  async function createElement() {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Drawer can open and close', async () => {
    ui = await createElement();

    const drawer = ui.shadowRoot.querySelector('#drawer');
    assertFalse(drawer.open);

    let menu = ui.shadowRoot.querySelector('#drawer os-settings-menu');
    assertEquals(null, menu);

    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    menu = ui.shadowRoot.querySelector('#drawer os-settings-menu');
    assertTrue(!!menu);

    drawer.cancel();
    // Drawer is closed, but menu is still stamped so its contents remain
    // visible as the drawer slides out.
    menu = ui.shadowRoot.querySelector('#drawer os-settings-menu');
    assertTrue(!!menu);
  });

  test('Drawer closes when exiting narrow mode', async () => {
    ui = await createElement();
    const drawer = ui.shadowRoot.querySelector('#drawer');

    // Mimic narrow mode and open the drawer.
    ui.isNarrow = true;
    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);
    assertTrue(drawer.open);

    // Mimic exiting narrow mode and confirm the drawer is closed
    ui.isNarrow = false;
    flush();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
  });

  test('Navigating via menu clears current search URL param', async () => {
    ui = await createElement();
    const settingsMenu = ui.shadowRoot.querySelector('os-settings-menu');

    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.shadowRoot.querySelector('iron-selector');
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    const router = Router.getInstance();
    router.navigateTo(routes.BASIC, urlParams);
    assertEquals(urlParams.toString(), router.getQueryParameters().toString());
    settingsMenu.shadowRoot.querySelector('a.item[href="personalization"]')
        .click();
    assertEquals('', router.getQueryParameters().toString());
  });

  test('Advanced section in menu and main page behavior', async () => {
    ui = await createElement();
    ui.shadowRoot.querySelector('#drawerTemplate').if = true;
    flush();

    const main = ui.shadowRoot.querySelector('os-settings-main');
    assertTrue(!!main);
    const mainPage = main.shadowRoot.querySelector('os-settings-page');
    assertTrue(!!mainPage);
    const mainPageAdvancedToggle =
        mainPage.shadowRoot.querySelector('#advancedToggle');
    assertTrue(!!mainPageAdvancedToggle);
    const floatingMenu = ui.shadowRoot.querySelector('#left os-settings-menu');
    assertTrue(!!floatingMenu);
    const drawerMenu = ui.shadowRoot.querySelector('#drawer os-settings-menu');
    assertTrue(!!drawerMenu);

    // Advanced section should not be expanded
    assertFalse(main.advancedToggleExpanded);
    assertFalse(drawerMenu.advancedOpened);
    assertFalse(floatingMenu.advancedOpened);

    mainPageAdvancedToggle.click();
    flush();

    // Advanced section should be expanded
    assertTrue(main.advancedToggleExpanded);
    assertTrue(drawerMenu.advancedOpened);
    assertTrue(floatingMenu.advancedOpened);

    // Collapse 'Advanced' in the menu.
    drawerMenu.$.advancedButton.click();
    flush();

    // Collapsing it in the menu should not collapse it in the main area.
    assertTrue(main.advancedToggleExpanded);
    assertFalse(drawerMenu.advancedOpened);
    assertFalse(floatingMenu.advancedOpened);

    // Expand both 'Advanced's again.
    drawerMenu.$.advancedButton.click();
    flush();

    // Collapse 'Advanced' in the main area.
    main.advancedToggleExpanded = false;
    flush();

    // Collapsing it in the main area should not collapse it in the menu.
    assertTrue(drawerMenu.advancedOpened);
    assertTrue(floatingMenu.advancedOpened);
    assertFalse(main.advancedToggleExpanded);
  });

  suite('When in kiosk mode', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isKioskModeActive: true,
      });
    });

    test('Menu is hidden', async () => {
      ui = await createElement();
      const menu = ui.shadowRoot.querySelector('os-settings-menu');
      assertEquals(null, menu);
    });
  });
});
