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

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createRouterForTesting, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsMenuElement, OsSettingsRoutes, OsSettingsUiElement, PageDisplayerElement, Router, routes, routesMojom, SettingsIdleLoadElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

suite('<os-settings-ui> page visibility', () => {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement;
  let mainPageContainer: MainPageContainerElement;
  let menu: OsSettingsMenuElement;
  let browserProxy: TestAccountManagerBrowserProxy;

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

  function queryMenuItemByPath(path: string): HTMLElement|null {
    return menu.shadowRoot!.querySelector<HTMLElement>(
        `os-settings-menu-item[path="${path}"]`);
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

  function isDefaultRouteElement(element: HTMLElement) {
    return element.getAttribute('route-path') === 'default';
  }

  /**
   * Verifies the page has a visible element (L1 page) for the default route,
   * and that any subpages are hidden.
   */
  function assertSubpagesHidden(section: routesMojom.Section): void {
    const pageDisplayer =
        mainPageContainer.shadowRoot!.querySelector<PageDisplayerElement>(
            `page-displayer[section="${section}"]`);
    assert(pageDisplayer);

    const pages = pageDisplayer.firstElementChild!.shadowRoot!.querySelector(
        'os-settings-animated-pages');
    assert(pages);

    const children =
        pages.shadowRoot!.querySelector('slot')!.assignedNodes({flatten: true})
            .filter(n => n.nodeType === Node.ELEMENT_NODE) as HTMLElement[];

    const stampedChildren = children.filter((element) => {
      return element.tagName !== 'TEMPLATE';
    });

    // The page's default route element should be stamped and visible.
    const defaultRouteElements = stampedChildren.filter(isDefaultRouteElement);
    assertEquals(
        1, defaultRouteElements.length,
        `Default route element not found for section ${section}`);
    const defaultRouteEl = defaultRouteElements[0];
    assert(defaultRouteEl);
    assertTrue(
        isVisible(defaultRouteEl),
        `Default route element for section ${section} should be visible.`);

    // Any other stamped subpages should not be visible.
    const subpages = stampedChildren.filter((element) => {
      return !isDefaultRouteElement(element);
    });
    for (const subpage of subpages) {
      assertFalse(
          isVisible(subpage),
          `Subpages for section ${section} should be hidden.`);
    }
  }

  suiteSetup(async () => {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
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
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('Document body has feature class when feature flag is enabled', () => {
    assertTrue(document.body.classList.contains('revamp-wayfinding-enabled'));
  });

  test('Network page should be the default visible page', () => {
    assertIsOnlyActiveAndVisiblePage(routesMojom.Section.kNetwork);
  });

  // Sort by order of menu items
  const routeNames: Array<keyof OsSettingsRoutes> = [
    'INTERNET',
    // Currently in the revamp, the Bluetooth L1 page automatically
    // redirects to the Bluetooth devices subpage.
    // TODO(b/309808834) Uncomment the line for the Bluetooth route below once
    // the L1 page is revamped and re-implemented.
    // 'BLUETOOTH',
    'MULTIDEVICE',
    'OS_PEOPLE',
    'KERBEROS',
    'DEVICE',
    'PERSONALIZATION',
    'OS_PRIVACY',
    'APPS',
    'OS_ACCESSIBILITY',
    'SYSTEM_PREFERENCES',
    'ABOUT',
  ];
  for (const routeName of routeNames) {
    test(
        `Clicking menu item for route ${routeName} should show that page only`,
        async () => {
          const route = routes[routeName];

          const menuItem = queryMenuItemByPath(route.path);
          assert(menuItem, `Menu item with path="${route.path}" not found.`);

          menuItem.click();
          await flushTasks();

          const section = route.section;
          assert(section !== null);  // Value can be 0 (valid enum value)
          assertIsOnlyActiveAndVisiblePage(section);
          assertPageIsFocused(section);
          assertSubpagesHidden(section);
        });
  }
});
