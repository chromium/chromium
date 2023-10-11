// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for the About page in the CrOS Settings UI.
 * Note: This suite assumes that the OsSettingsRevampWayfinding feature flag
 * is disabled.
 */

import 'chrome://os-settings/os_settings.js';

import {CrSettingsPrefs, MainPageContainerElement, OsSettingsMenuElement, OsSettingsUiElement, Router, routes, routesMojom, SettingsIdleLoadElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<os-settings-ui> About page', () => {
  let ui: OsSettingsUiElement;
  let mainPageContainer: MainPageContainerElement;
  let menu: OsSettingsMenuElement;

  async function createUi() {
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();
    await CrSettingsPrefs.initialized;

    const mainEl = ui.shadowRoot!.querySelector('os-settings-main');
    assert(mainEl);

    const mainPageContainerEl =
        mainEl.shadowRoot!.querySelector('main-page-container');
    assert(mainPageContainerEl);
    mainPageContainer = mainPageContainerEl;

    const menuEl = ui.shadowRoot!.querySelector<OsSettingsMenuElement>(
        '#left os-settings-menu');
    assert(menuEl);
    menu = menuEl;
    menu.advancedOpened = true;
    flush();

    // Force load advanced page container
    const advancedPageTemplateEl =
        mainPageContainer.shadowRoot!.querySelector<SettingsIdleLoadElement>(
            '#advancedPageTemplate');
    assert(advancedPageTemplateEl);
    await advancedPageTemplateEl.get();
    flush();
  }

  function queryMenuItemByPath(path: string): HTMLElement|null {
    return menu.shadowRoot!.querySelector<HTMLElement>(
        `os-settings-menu-item[path="${path}"]`);
  }

  async function clickMenuItemAndWaitForPage(menuItem: HTMLElement):
      Promise<void> {
    const pageVisiblePromise = eventToPromise('show-container', window);
    menuItem.click();
    flush();
    await pageVisiblePromise;
  }

  setup(async () => {
    Router.getInstance().navigateTo(routes.BASIC);
    await createUi();
  });

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'About page and Basic page visibility are mutually exclusive',
      async () => {
        const basicPageContainer =
            mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
                '#basicPageContainer');
        assertTrue(!!basicPageContainer);
        const advancedPageContainer =
            mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
                '#advancedPageContainer');
        assertTrue(!!advancedPageContainer);
        const aboutPageContainer =
            mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
                '#aboutPageContainer');
        assertTrue(!!aboutPageContainer);

        // Show basic page
        let menuItem =
            queryMenuItemByPath(`/${routesMojom.BLUETOOTH_SECTION_PATH}`);
        assertTrue(!!menuItem);
        await clickMenuItemAndWaitForPage(menuItem);
        assertTrue(isVisible(basicPageContainer));
        assertTrue(isVisible(advancedPageContainer));
        assertFalse(isVisible(aboutPageContainer));

        // Show about page
        menuItem =
            queryMenuItemByPath(`/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`);
        assertTrue(!!menuItem);
        await clickMenuItemAndWaitForPage(menuItem);
        assertFalse(isVisible(basicPageContainer));
        assertFalse(isVisible(advancedPageContainer));
        assertTrue(isVisible(aboutPageContainer));

        // Show advanced page
        menuItem =
            queryMenuItemByPath(`/${routesMojom.DATE_AND_TIME_SECTION_PATH}`);
        assertTrue(!!menuItem);
        await clickMenuItemAndWaitForPage(menuItem);
        assertTrue(isVisible(basicPageContainer));
        assertTrue(isVisible(advancedPageContainer));
        assertFalse(isVisible(aboutPageContainer));

        // Show about page
        menuItem =
            queryMenuItemByPath(`/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`);
        assertTrue(!!menuItem);
        await clickMenuItemAndWaitForPage(menuItem);
        assertFalse(isVisible(basicPageContainer));
        assertFalse(isVisible(advancedPageContainer));
        assertTrue(isVisible(aboutPageContainer));
      });
});
