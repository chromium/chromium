// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrDrawerElement, CrSettingsPrefs, OsSettingsMenuElement, OsSettingsUiElement, Router, routes, routesMojom, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomIf, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

suite('<os-settings-ui> menu', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let ui: OsSettingsUiElement;
  let fakeNearbySettings: FakeNearbyShareSettings;
  let testAccountManagerBrowserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(() => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);

    // Setup fake accounts.
    testAccountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        testAccountManagerBrowserProxy);
  });

  async function createElement(): Promise<OsSettingsUiElement> {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
    testAccountManagerBrowserProxy.reset();
  });

  test('Drawer can open and close', async () => {
    ui = await createElement();

    const drawer = ui.shadowRoot!.querySelector<CrDrawerElement>('#drawer');
    assertTrue(!!drawer);
    assertFalse(drawer.open);

    let menu = ui.shadowRoot!.querySelector('#drawer os-settings-menu');
    assertNull(menu);

    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    menu = ui.shadowRoot!.querySelector('#drawer os-settings-menu');
    assertTrue(!!menu);

    drawer.cancel();
    // Drawer is closed, but menu is still stamped so its contents remain
    // visible as the drawer slides out.
    menu = ui.shadowRoot!.querySelector('#drawer os-settings-menu');
    assertTrue(!!menu);
  });

  test('Drawer closes when exiting narrow mode', async () => {
    ui = await createElement();
    const drawer = ui.shadowRoot!.querySelector<CrDrawerElement>('#drawer');
    assertTrue(!!drawer);

    // Mimic narrow mode and open the drawer.
    ui.set('isNarrow', true);
    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);
    assertTrue(drawer.open);

    // Mimic exiting narrow mode and confirm the drawer is closed
    ui.set('isNarrow', false);
    flush();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
  });

  test('Navigating via menu clears current search URL param', async () => {
    ui = await createElement();
    const settingsMenu = ui.shadowRoot!.querySelector('os-settings-menu');
    assertTrue(!!settingsMenu);

    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector =
        settingsMenu.shadowRoot!.querySelector('iron-selector');
    assertTrue(!!ironSelector);
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    const router = Router.getInstance();
    router.navigateTo(routes.BASIC, urlParams);
    assertEquals(urlParams.toString(), router.getQueryParameters().toString());
    const personalizationPath = `/${routesMojom.PERSONALIZATION_SECTION_PATH}`;
    const link = settingsMenu.shadowRoot!.querySelector<HTMLAnchorElement>(
        `os-settings-menu-item[path="${personalizationPath}"]`);
    assertTrue(!!link);
    link.click();
    assertEquals('', router.getQueryParameters().toString());
  });

  if (!isRevampWayfindingEnabled) {
    test('Advanced section in menu and main page behavior', async () => {
      ui = await createElement();
      const drawerTemplate =
          ui.shadowRoot!.querySelector<DomIf>('#drawerTemplate');
      assertTrue(!!drawerTemplate);
      drawerTemplate.if = true;
      flush();

      const main = ui.shadowRoot!.querySelector('os-settings-main');
      assertTrue(!!main);
      const mainPageContainer =
          main.shadowRoot!.querySelector('main-page-container');
      assertTrue(!!mainPageContainer);
      const mainPageAdvancedToggle =
          mainPageContainer.shadowRoot!.querySelector<HTMLButtonElement>(
              '#advancedToggle');
      assertTrue(!!mainPageAdvancedToggle);
      const floatingMenu = ui.shadowRoot!.querySelector<OsSettingsMenuElement>(
          '#left os-settings-menu');
      assertTrue(!!floatingMenu);
      const drawerMenu = ui.shadowRoot!.querySelector<OsSettingsMenuElement>(
          '#drawer os-settings-menu');
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
      const advancedButton =
          drawerMenu.shadowRoot!.querySelector<HTMLButtonElement>(
              '#advancedButton');
      assertTrue(!!advancedButton);
      advancedButton.click();
      flush();

      // Collapsing it in the menu should not collapse it in the main area.
      assertTrue(main.advancedToggleExpanded);
      assertFalse(drawerMenu.advancedOpened);
      assertFalse(floatingMenu.advancedOpened);

      // Expand both 'Advanced's again.
      advancedButton.click();
      flush();

      // Collapse 'Advanced' in the main area.
      main.set('advancedToggleExpanded', false);
      flush();

      // Collapsing it in the main area should not collapse it in the menu.
      assertTrue(drawerMenu.advancedOpened);
      assertTrue(floatingMenu.advancedOpened);
      assertFalse(main.advancedToggleExpanded);
    });
  }

  suite('When in kiosk mode', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isKioskModeActive: true,
      });
    });

    test('Menu is hidden', async () => {
      ui = await createElement();
      const menu = ui.shadowRoot!.querySelector('os-settings-menu');
      assertNull(menu);
    });
  });
});
