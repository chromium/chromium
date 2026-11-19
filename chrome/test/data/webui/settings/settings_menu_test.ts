// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type {SettingsMenuElement, SettingsRoutes} from 'chrome://settings/settings.js';
import {AutofillSettingsReferrer, resetRouterForTesting, loadTimeData, MetricsBrowserProxyImpl, resetPageVisibilityForTesting, Router} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

// Asserts that each visible menu item shows the icon of the route it links to.
function assertMenuIconsMatchRoutes(menu: SettingsMenuElement) {
  const links = menu.shadowRoot.querySelectorAll<HTMLAnchorElement>(
      'cr-menu-selector a[role=menuitem]:not(#extensionsLink):not([hidden])');
  assertTrue(links.length > 0);
  for (const link of links) {
    const path = link.getAttribute('href')!;
    const route = Router.getInstance().getRouteForPath(path);
    assertTrue(!!route, path);
    const icon = link.querySelector<CrIconElement>('cr-icon');
    assertTrue(!!icon, path);
    assertEquals(route.getIcon(), icon.icon, path);
    // No <svg> is rendered if the iconset doesn't have an icon of that name.
    assertTrue(!!icon.shadowRoot.querySelector('svg'), path);
  }
}

suite('SettingsMenu', function() {
  let settingsMenu: SettingsMenuElement;
  let routes: SettingsRoutes;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  function createSettingsMenu() {
    routes = Router.getInstance().getRoutes();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
  }

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    createSettingsMenu();
  });

  teardown(function() {
    resetPageVisibilityForTesting();
  });

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', async () => {
    const urlParams = new URLSearchParams('search=foo');
    Router.getInstance().navigateTo(
        Router.getInstance().getRoutes().BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());

    const selector = settingsMenu.$.menu;
    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    const people =
        settingsMenu.shadowRoot.querySelector<HTMLElement>('#people');
    assertTrue(!!people);
    people.click();
    const event = await whenIronSelect;

    assertEquals(people, event.detail.item);
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });

  test('openResetSection', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected);
  });

  test('navigateToAnotherSection', async function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected);

    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    Router.getInstance().navigateTo(routes.PEOPLE);
    const event = await whenIronSelect;

    const people =
        settingsMenu.shadowRoot.querySelector<HTMLElement>('#people');
    assertTrue(!!people);
    assertTrue(!!selector.selected);
    assertEquals(people, event.detail.item);
    assertEquals('/people', selector.selected);
  });

  test('navigateToBasic', async function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected);

    Router.getInstance().navigateTo(routes.BASIC);
    await microtasksFinished();
    assertFalse(!!selector.selected);
  });

  test('menuIconsMatchRoutes', async function() {
    await microtasksFinished();
    assertMenuIconsMatchRoutes(settingsMenu);
  });

  test('noExperimental', async function() {
    loadTimeData.overrideValues({showAiPage: false});
    resetRouterForTesting();
    createSettingsMenu();
    await microtasksFinished();

    const entry = settingsMenu.shadowRoot.querySelector('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertFalse(isVisible(entry));
  });

  test('navigateToExperimental', async function() {
    loadTimeData.overrideValues({showAiPage: true});
    resetRouterForTesting();
    createSettingsMenu();
    const selector = settingsMenu.$.menu;

    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    Router.getInstance().navigateTo(routes.AI);
    const event = await whenIronSelect;

    const entry = settingsMenu.shadowRoot.querySelector('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertTrue(isVisible(entry));

    assertTrue(!!event.detail.item);
    assertEquals('/ai', selector.selected?.toString());
    assertEquals('/ai', event.detail.item.getAttribute('href'));
  });

  test('pageVisibility', function() {
    function assertPagesHidden(expectedHidden: boolean) {
      const ids = [
        'accessibility', 'appearance',
        // <if expr="not is_chromeos">
        'defaultBrowser',
        // </if>
        'downloads', 'languages', 'onStartup', 'people', 'performance', 'reset',
        // <if expr="not is_chromeos">
        'system',
        // </if>
      ];

      for (const id of ids) {
        assertEquals(
            expectedHidden,
            settingsMenu.shadowRoot.querySelector<HTMLElement>(
                                       `#${id}`)!.hidden);
      }
    }

    // The default pageVisibility should not cause menu items to be hidden.
    assertPagesHidden(false);

    // Set the visibility of the pages under test to "false".
    resetPageVisibilityForTesting({
      a11y: false,
      appearance: false,
      defaultBrowser: false,
      downloads: false,
      languages: false,
      onStartup: false,
      people: false,
      performance: false,
      reset: false,
      system: false,
    });
    createSettingsMenu();

    // Now, the menu items should be hidden.
    assertPagesHidden(true);
  });

  test('aiPageMenuClick', async function() {
    loadTimeData.overrideValues({
      showAiPage: true,
    });
    resetRouterForTesting();
    createSettingsMenu();
    await microtasksFinished();

    const entry =
        settingsMenu.shadowRoot.querySelector<HTMLElement>('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertTrue(isVisible(entry));

    const selector = settingsMenu.$.menu;
    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    entry.click();
    // Ensure UMA is logged.
    assertEquals(
        'SettingsMenu_AiPageEntryPointClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
    await whenIronSelect;

    assertEquals(routes.AI, Router.getInstance().getCurrentRoute());
  });

  test('yourSavedInfoMenuItemClick', async function() {
    resetRouterForTesting();
    createSettingsMenu();
    await microtasksFinished();

    const selector = settingsMenu.$.menu;
    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    const entry = settingsMenu.shadowRoot.querySelector<HTMLElement>(
        'a[href=\'/autofill\']');
    assertTrue(!!entry);
    assertTrue(isVisible(entry));

    entry.click();
    await whenIronSelect;
    const [histogramName, referrer] =
        await metricsBrowserProxy.whenCalled('recordAutofillSettingsReferrer');
    assertEquals(
        'Autofill.YourSavedInfoSettingsPage.VisitReferrer', histogramName);
    assertEquals(AutofillSettingsReferrer.SETTINGS_MENU, referrer);
    assertTrue(!!selector.selected);
    assertEquals('/autofill', selector.selected);
    assertEquals(
        routes.AUTOFILL, Router.getInstance().getCurrentRoute());
  });

  test('navMenuItemClickActions', async function() {
    loadTimeData.overrideValues({
      showAiPage: true,
      isGuest: false,
    });
    resetRouterForTesting();
    createSettingsMenu();
    await microtasksFinished();

    const testCases = [
      {
        selector: '#people',
        action: 'SettingsMenu_PeopleClicked',
        route: routes.PEOPLE,
      },
      {
        selector: '#autofill',
        action: 'SettingsMenu_AutofillClicked',
        route: routes.AUTOFILL,
      },
      {
        selector: '#privacy',
        action: 'SettingsMenu_PrivacyClicked',
        route: routes.PRIVACY,
      },
      {
        selector: '#performance',
        action: 'SettingsMenu_PerformanceClicked',
        route: routes.PERFORMANCE,
      },
      {
        selector: '#ai',
        action: 'SettingsMenu_AiPageEntryPointClicked',
        route: routes.AI,
      },
      {
        selector: '#appearance',
        action: 'SettingsMenu_AppearanceClicked',
        route: routes.APPEARANCE,
      },
      {
        selector: '#search',
        action: 'SettingsMenu_SearchClicked',
        route: routes.SEARCH,
      },
      {
        selector: '#onStartup',
        action: 'SettingsMenu_OnStartupClicked',
        route: routes.ON_STARTUP,
      },
      {
        selector: '#languages',
        action: 'SettingsMenu_LanguagesClicked',
        route: routes.LANGUAGES,
      },
      {
        selector: '#downloads',
        action: 'SettingsMenu_DownloadsClicked',
        route: routes.DOWNLOADS,
      },
      {
        selector: '#accessibility',
        action: 'SettingsMenu_AccessibilityClicked',
        route: routes.ACCESSIBILITY,
      },
      {
        selector: '#reset',
        action: 'SettingsMenu_ResetClicked',
        route: routes.RESET,
      },
      {
        selector: '#about-menu',
        action: 'SettingsMenu_AboutClicked',
        route: routes.ABOUT,
      },
      // <if expr="not is_chromeos">
      {
        selector: '#defaultBrowser',
        action: 'SettingsMenu_DefaultBrowserClicked',
        route: routes.DEFAULT_BROWSER,
      },
      {
        selector: '#system',
        action: 'SettingsMenu_SystemClicked',
        route: routes.SYSTEM,
      },
      // </if>
    ];

    for (const testCase of testCases) {
      metricsBrowserProxy.resetResolver('recordAction');
      metricsBrowserProxy.resetResolver('recordSettingsNavCategoryClicked');

      const navItem =
          settingsMenu.shadowRoot.querySelector<HTMLElement>(testCase.selector);
      assertTrue(!!navItem);
      navItem.click();

      const action = await metricsBrowserProxy.whenCalled('recordAction');
      assertEquals(testCase.action, action);
      assertEquals(testCase.route, Router.getInstance().getCurrentRoute());
      assertEquals(
          1,
          metricsBrowserProxy.getCallCount('recordSettingsNavCategoryClicked'));
    }
  });
});

suite('SettingsMenuAutofill', () => {
  let settingsMenu: SettingsMenuElement;

  function createSettingsMenu() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
  }

  test('Update yourSavedInfo visibility', async () => {
    resetRouterForTesting();
    createSettingsMenu();
    await microtasksFinished();

    const autofillEntry = settingsMenu.shadowRoot.querySelector<HTMLElement>(
        'a[href=\'/autofill\']');
    assertTrue(!!autofillEntry);
    assertTrue(isVisible(autofillEntry));

    // Hide the your saved info page.
    resetPageVisibilityForTesting({
      yourSavedInfo: false,
    });
    createSettingsMenu();
    const newAutofillEntry = settingsMenu.shadowRoot.querySelector<HTMLElement>(
        'a[href=\'/autofill\']');
    assertTrue(!!newAutofillEntry);
    assertFalse(isVisible(newAutofillEntry));
  });
});

suite('SettingsMenuRefresh2026', () => {
  let settingsMenu: SettingsMenuElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(() => {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    loadTimeData.overrideValues({
      isGuest: false,
      settingsRefresh2026: 'settings-refresh-2026',
      showAiPage: true,
    });
    resetPageVisibilityForTesting();
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    return microtasksFinished();
  });

  test('menuIconsMatchRoutes', () => {
    assertMenuIconsMatchRoutes(settingsMenu);
  });

  test('menuItemsOrderAndNavigation', async () => {
    const links =
        Array.from(settingsMenu.shadowRoot.querySelectorAll<HTMLAnchorElement>(
            'cr-menu-selector a[role=menuitem]'));
    const hrefs = links.map(link => link.getAttribute('href'));
    assertDeepEquals(
        [
          '/',
          '/autofill',
          '/privacy',
          '/performance',
          '/ai',
          '/appearance',
          '/languages',
          '/accessibility',
          '/search',
          '/general',
          'chrome://extensions',
          '/help',
        ],
        hrefs);

    const selector = settingsMenu.$.menu;
    assertEquals('/', selector.selected);

    let whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    const general =
        settingsMenu.shadowRoot.querySelector<HTMLElement>('#general');
    assertTrue(!!general);
    general.click();
    await whenIronSelect;
    assertEquals('/general', selector.selected);
    assertEquals(
        Router.getInstance().getRoutes().GENERAL,
        Router.getInstance().getCurrentRoute());
    assertEquals(
        'SettingsMenu_GeneralClicked',
        await testMetricsBrowserProxy.whenCalled('recordAction'));

    testMetricsBrowserProxy.resetResolver('recordAction');
    whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    const home = settingsMenu.shadowRoot.querySelector<HTMLElement>('#home');
    assertTrue(!!home);
    home.click();
    await whenIronSelect;
    assertEquals('/', selector.selected);
    assertEquals(
        Router.getInstance().getRoutes().BASIC,
        Router.getInstance().getCurrentRoute());
    assertEquals('/', window.location.pathname);
    assertEquals(
        'SettingsMenu_HomeClicked',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('managementLink', async () => {
    assertFalse(!!settingsMenu.shadowRoot.querySelector('#managementLink'));

    loadTimeData.overrideValues({
      isManaged: true,
      managedByIcon: 'cr:domain',
      managementPageUrl: 'chrome://management',
      browserManagedByOrg:
          'Your <a href="chrome://management">browser is managed</a> by ' +
          'google.com',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    await microtasksFinished();

    const management = settingsMenu.shadowRoot.querySelector('#management');
    assertTrue(!!management);
    assertFalse(management.classList.contains('cr-nav-menu-item'));
    assertFalse(!!settingsMenu.shadowRoot.querySelector('#managementIcon'));

    const managementLink =
        settingsMenu.shadowRoot.querySelector<HTMLAnchorElement>(
            '#managementLink');
    assertTrue(!!managementLink);
    assertEquals('chrome://management', managementLink.getAttribute('href'));
    assertEquals('_blank', managementLink.target);
    assertEquals(0, managementLink.tabIndex);
    assertEquals('browser is managed', managementLink.textContent);
    assertFalse(managementLink.classList.contains('cr-nav-menu-item'));

    const label =
        settingsMenu.shadowRoot.querySelector('#management .menu-label');
    assertTrue(!!label);
    assertEquals(
        'Your browser is managed by google.com', label.textContent.trim());

    const baseHrefs = [
      '/',
      '/autofill',
      '/privacy',
      '/performance',
      '/ai',
      '/appearance',
      '/languages',
      '/accessibility',
      '/search',
      '/general',
      'chrome://extensions',
      '/help',
    ];
    const getMenuHrefs = () =>
        Array
            .from(settingsMenu.shadowRoot.querySelectorAll<HTMLAnchorElement>(
                'cr-menu-selector a[role=menuitem]'))
            .map(link => link.getAttribute('href'));
    assertDeepEquals(baseHrefs, getMenuHrefs());

    const homeItem =
        settingsMenu.shadowRoot.querySelector<HTMLElement>('#home');
    const aboutItem =
        settingsMenu.shadowRoot.querySelector<HTMLElement>('#about-menu');
    assertTrue(!!homeItem);
    assertTrue(!!aboutItem);
    homeItem.focus();
    assertEquals(homeItem, settingsMenu.shadowRoot.activeElement);
    homeItem.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
    assertEquals(aboutItem, settingsMenu.shadowRoot.activeElement);
    aboutItem.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
    assertEquals(homeItem, settingsMenu.shadowRoot.activeElement);

    document.documentElement.toggleAttribute('settings-refresh-2026', true);
    try {
      settingsMenu.style.width = '55px';
      settingsMenu.style.overflow = 'auto';
      settingsMenu.collapsed = true;
      await microtasksFinished();

      assertFalse(!!settingsMenu.shadowRoot.querySelector('#management'));
      const managementIcon =
          settingsMenu.shadowRoot.querySelector<HTMLAnchorElement>(
              '#managementIcon');
      assertTrue(!!managementIcon);
      assertEquals('menuitem', managementIcon.getAttribute('role'));
      assertTrue(managementIcon.classList.contains('cr-nav-menu-item'));
      assertEquals('chrome://management', managementIcon.getAttribute('href'));
      assertEquals('_blank', managementIcon.target);

      assertEquals(settingsMenu.clientWidth, settingsMenu.scrollWidth);
      const iconRect = managementIcon.getBoundingClientRect();
      const aboutRect = aboutItem.getBoundingClientRect();
      assertEquals(aboutRect.width, iconRect.width);
      assertEquals(aboutRect.height, iconRect.height);

      assertDeepEquals([...baseHrefs, 'chrome://management'], getMenuHrefs());
      assertFalse(
          settingsMenu.$.menu.getItemsForTest().includes(managementIcon));

      aboutItem.focus();
      aboutItem.dispatchEvent(
          new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
      assertEquals(managementIcon, settingsMenu.shadowRoot.activeElement);
      managementIcon.dispatchEvent(
          new KeyboardEvent('keydown', {key: 'ArrowDown', bubbles: true}));
      assertEquals(homeItem, settingsMenu.shadowRoot.activeElement);
    } finally {
      document.documentElement.removeAttribute('settings-refresh-2026');
    }
  });
});
