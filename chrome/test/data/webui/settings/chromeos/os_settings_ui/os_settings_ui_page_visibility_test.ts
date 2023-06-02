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

import {createRoutesForTesting, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsMenuElement, OsSettingsSectionElement, OsSettingsUiElement, Router} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

    const idleRender =
        mainPageContainer.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  }

  function queryActivePages(): NodeListOf<OsSettingsSectionElement> {
    return mainPageContainer.shadowRoot!
        .querySelectorAll<OsSettingsSectionElement>(
            `os-settings-section[active]`);
  }

  function queryMenuItem(pageName: string): HTMLElement|null {
    return menu.shadowRoot!.querySelector<HTMLElement>(
        `a.item[data-page-name='${pageName}']`);
  }

  suiteSetup(async () => {
    loadTimeData.overrideValues({
      isRevampWayfindingEnabled: true,
      isKerberosEnabled: true,  // Simulate kerberos page available
    });

    // Recreate routes and Router so Kerberos route exists
    const testRoutes = createRoutesForTesting();
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

  const pageNames = [
    'apps',
    'bluetooth',
    'crostini',
    'dateTime',
    'device',
    'files',
    'internet',
    'kerberos',
    'multidevice',
    'osAccessibility',
    'osLanguages',
    'osPeople',
    'osPrinting',
    'osPrivacy',
    'osReset',
    'osSearch',
    'personalization',
  ];
  for (const pageName of pageNames) {
    test(
        `Clicking menu item for ${pageName} page should show that page`,
        async () => {
          const pageReadyPromise = eventToPromise('show-container', window);

          const menuItem = queryMenuItem(pageName);
          assert(menuItem);
          menuItem.click();
          flush();

          await pageReadyPromise;

          const activePages = queryActivePages();
          assertEquals(1, activePages.length);

          const page = activePages[0];
          assert(page);
          assertEquals(pageName, page.section);
          assertNotEquals('none', getComputedStyle(page).display);
        });
  }
});
