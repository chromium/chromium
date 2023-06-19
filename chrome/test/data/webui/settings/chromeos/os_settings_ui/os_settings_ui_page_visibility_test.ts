// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for page visibility in the CrOS Settings UI. These tests
 * expect the OsSettingsRevampWayfinding feature flag to be enabled.
 * Separated into a separate file to mitigate test timeouts.
 */

import 'chrome://os-settings/os_settings.js';

import {createRoutesForTesting, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsMenuElement, OsSettingsRoutes, OsSettingsUiElement, Router, routesMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const {Section} = routesMojom;
type PageName = keyof typeof Section;

suite('<os-settings-ui> page visibility', () => {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement;
  let mainPageContainer: MainPageContainerElement;
  let menu: OsSettingsMenuElement;
  let testRoutes: OsSettingsRoutes;

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

    const idleRender =
        mainPageContainer.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  }

  function queryMenuItem(pageName: PageName): HTMLElement|null {
    return menu.shadowRoot!.querySelector<HTMLElement>(
        `a.item[data-section="${Section[pageName]}"]`);
  }

  /**
   * Asserts the following:
   * - Only one page is marked active
   * - Active page does not have style "display: none"
   * - Active page is focused
   * - Inactive pages have style "display: none"
   */
  function assertOnlyActivePageIsVisible(pageName: PageName): void {
    const pages =
        mainPageContainer.shadowRoot!.querySelectorAll('page-displayer');
    let numActive = 0;

    for (const page of pages) {
      const displayStyle = getComputedStyle(page).display;
      if (page.hasAttribute('active')) {
        numActive++;
        assertNotEquals('none', displayStyle);
        assertEquals(Section[pageName], page.section);
        assertEquals(page, mainPageContainer.shadowRoot!.activeElement);
      } else {
        assertEquals('none', displayStyle);
      }
    }

    assertEquals(1, numActive);
  }

  suiteSetup(async () => {
    loadTimeData.overrideValues({
      isRevampWayfindingEnabled: true,
      isKerberosEnabled: true,  // Simulate kerberos page available
    });

    // Recreate routes and Router so Kerberos route exists
    testRoutes = createRoutesForTesting();
    Router.resetInstanceForTesting(new Router(testRoutes));

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
    Router.getInstance().navigateTo(testRoutes.BASIC);
    assertOnlyActivePageIsVisible('kNetwork');
  });

  const pageNames: PageName[] = [
    'kAccessibility',
    'kApps',
    'kBluetooth',
    'kCrostini',
    'kDateAndTime',
    'kDevice',
    'kFiles',
    'kKerberos',
    'kMultiDevice',
    'kLanguagesAndInput',
    'kNetwork',
    'kPeople',
    'kPersonalization',
    'kPrinting',
    'kPrivacyAndSecurity',
    'kReset',
    'kSearchAndAssistant',
  ];
  for (const pageName of pageNames) {
    test(
        `Clicking menu item for ${pageName} page should show only that page`,
        async () => {
          const pageReadyPromise = eventToPromise('show-container', window);

          const menuItem = queryMenuItem(pageName);
          assert(menuItem);
          menuItem.click();
          flush();

          await pageReadyPromise;

          assertOnlyActivePageIsVisible(pageName);
        });
  }
});
