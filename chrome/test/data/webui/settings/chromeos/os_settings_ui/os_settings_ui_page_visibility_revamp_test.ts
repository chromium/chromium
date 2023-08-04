// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for page visibility in the CrOS Settings UI.
 *
 * - These tests expect the OsSettingsRevampWayfinding feature flag to be
 *   enabled.
 * - This suite is separated into a dedicated file to mitigate test timeouts
 *   since the element is very large.
 */

import 'chrome://os-settings/os_settings.js';

import {createRouterForTesting, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsMenuElement, OsSettingsRoutes, OsSettingsUiElement, Router, routes, routesMojom, SettingsIdleLoadElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<os-settings-ui> page visibility', () => {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement;
  let mainPageContainer: MainPageContainerElement;
  let menu: OsSettingsMenuElement;

  async function createUi() {
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();
    await CrSettingsPrefs.initialized;

    const mainEl = ui.shadowRoot!.querySelector('os-settings-main');
    assert(mainEl);
    settingsMain = mainEl;

    const mainPageContainerEl =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assert(mainPageContainerEl);
    mainPageContainer = mainPageContainerEl;

    const menuEl = ui.shadowRoot!.querySelector<OsSettingsMenuElement>(
        '#left os-settings-menu');
    assert(menuEl);
    menu = menuEl;

    // Force load advanced page container
    const advancedPageTemplate =
        mainPageContainer.shadowRoot!.querySelector<SettingsIdleLoadElement>(
            '#advancedPageTemplate');
    assert(advancedPageTemplate);
    await advancedPageTemplate.get();
    flush();
  }

  function queryMenuItemByHref(href: string): HTMLElement|null {
    return menu.shadowRoot!.querySelector<HTMLElement>(
        `a.item[href="${href}"]`);
  }

  /**
   * Asserts the following:
   * - The page for |section| is the only page marked active
   * - The page for |section| is the only page visible
   */
  function assertIsOnlyActiveAndVisiblePage(section: routesMojom.Section):
      void {
    const pages =
        mainPageContainer.shadowRoot!.querySelectorAll('page-displayer');
    for (const page of pages) {
      if (page.section === section) {
        assertTrue(page.active);
        assertTrue(isVisible(page));
      } else {
        assertFalse(page.active);
        assertFalse(isVisible(page));
      }
    }
  }

  /**
   * Asserts the page with the given |section| is focused.
   */
  function assertPageIsFocused(section: routesMojom.Section): void {
    const page = mainPageContainer.shadowRoot!.querySelector(
        `page-displayer[section="${section}"`);
    assertEquals(page, mainPageContainer.shadowRoot!.activeElement);
  }

  suiteSetup(async () => {
    assertTrue(
        loadTimeData.getBoolean('isRevampWayfindingEnabled'),
        'This suite expects OsSettingsRevampWayfinding to be enabled.');
    loadTimeData.overrideValues({
      isKerberosEnabled: true,  // Simulate kerberos route exists
    });

    // Reinitialize Router and routes based on load time data
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);

    await createUi();
  });

  suiteTeardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Document body has feature class when feature flag is enabled', () => {
    assertTrue(document.body.classList.contains('revamp-wayfinding-enabled'));
  });

  test('Network page should be the default visible page', () => {
    assertIsOnlyActiveAndVisiblePage(routesMojom.Section.kNetwork);
  });

  // Sort by order of menu items
  // TODO(b/292678609) Include test for kSystemPreferences Section once there
  // is a corresponding L1 page.
  const routeNames: Array<keyof OsSettingsRoutes> = [
    'INTERNET',
    'BLUETOOTH',
    'MULTIDEVICE',
    'OS_PEOPLE',
    'KERBEROS',
    'DEVICE',
    'PERSONALIZATION',
    'OS_PRIVACY',
    'APPS',
    'OS_ACCESSIBILITY',
    'ABOUT',
  ];
  for (const routeName of routeNames) {
    test(
        `Clicking menu item for route ${routeName} should show that page only`,
        async () => {
          const route = routes[routeName];

          const menuItem = queryMenuItemByHref(route.path);
          assert(menuItem, `Menu item with href="${route.path}" not found.`);

          const pageReadyPromise = eventToPromise('show-container', window);
          menuItem.click();
          flush();
          await pageReadyPromise;

          const section = route.section;
          assert(section !== null);  // Value can be 0 (valid enum value)
          assertIsOnlyActiveAndVisiblePage(section);
          assertPageIsFocused(section);
        });
  }
});
