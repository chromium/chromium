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

import {createRouterForTesting, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsMenuElement, OsSettingsUiElement, Router, routesMojom, SettingsIdleLoadElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

const {Section} = routesMojom;
type SectionName = keyof typeof Section;

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
   * - The page with |sectionName| is the only page marked active
   * - The page with |sectionName| is the only page visible
   */
  function assertIsOnlyActiveAndVisiblePage(sectionName: SectionName): void {
    const pages =
        mainPageContainer.shadowRoot!.querySelectorAll('page-displayer');
    for (const page of pages) {
      if (page.section === Section[sectionName]) {
        assertTrue(page.active);
        assertTrue(isVisible(page));
      } else {
        assertFalse(page.active);
        assertFalse(isVisible(page));
      }
    }
  }

  /**
   * Asserts the page with the given |sectionName| is focused.
   */
  function assertPageIsFocused(sectionName: SectionName): void {
    const page = mainPageContainer.shadowRoot!.querySelector(
        `page-displayer[section="${Section[sectionName]}"`);
    assertEquals(page, mainPageContainer.shadowRoot!.activeElement);
  }

  suiteSetup(async () => {
    loadTimeData.overrideValues({
      isRevampWayfindingEnabled: true,
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
    assertIsOnlyActiveAndVisiblePage('kNetwork');
  });

  interface MenuItemData {
    sectionName: SectionName;
    href: string;
  }

  const menuItemData: MenuItemData[] = [
    // Basic pages
    {
      sectionName: 'kNetwork',
      href: `/${routesMojom.NETWORK_SECTION_PATH}`,
    },
    {
      sectionName: 'kBluetooth',
      href: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
    },
    {
      sectionName: 'kMultiDevice',
      href: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
    },
    {
      sectionName: 'kPeople',
      href: `/${routesMojom.PEOPLE_SECTION_PATH}`,
    },
    {
      sectionName: 'kKerberos',
      href: `/${routesMojom.KERBEROS_SECTION_PATH}`,
    },
    {
      sectionName: 'kDevice',
      href: `/${routesMojom.DEVICE_SECTION_PATH}`,
    },
    {
      sectionName: 'kPersonalization',
      href: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
    },
    {
      sectionName: 'kSearchAndAssistant',
      href: `/${routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH}`,
    },
    {
      sectionName: 'kPrivacyAndSecurity',
      href: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
    },
    {
      sectionName: 'kApps',
      href: `/${routesMojom.APPS_SECTION_PATH}`,
    },
    {
      sectionName: 'kAccessibility',
      href: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
    },

    // Advanced pages
    {
      sectionName: 'kDateAndTime',
      href: `/${routesMojom.DATE_AND_TIME_SECTION_PATH}`,
    },
    {
      sectionName: 'kLanguagesAndInput',
      href: `/${routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH}`,
    },
    {
      sectionName: 'kFiles',
      href: `/${routesMojom.FILES_SECTION_PATH}`,
    },
    {
      sectionName: 'kPrinting',
      href: `/${routesMojom.PRINTING_SECTION_PATH}`,
    },
    {
      sectionName: 'kCrostini',
      href: `/${routesMojom.CROSTINI_SECTION_PATH}`,
    },
    {
      sectionName: 'kReset',
      href: `/${routesMojom.RESET_SECTION_PATH}`,
    },
  ];

  for (const {sectionName, href} of menuItemData) {
    test(
        `Clicking menu item for ${sectionName} page should show only that page`,
        async () => {
          const menuItem = queryMenuItemByHref(href);
          assert(menuItem);

          const pageReadyPromise = eventToPromise('show-container', window);
          menuItem.click();
          flush();
          await pageReadyPromise;

          assertIsOnlyActiveAndVisiblePage(sectionName);
          assertPageIsFocused(sectionName);
        });
  }
});
