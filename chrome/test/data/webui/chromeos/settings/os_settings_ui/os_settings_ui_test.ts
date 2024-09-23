// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for the overall OS Settings UI.
 */

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrDrawerElement, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsUiElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';
import {clearBody} from '../utils.js';

suite('OSSettingsUi', () => {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement|null;
  let mainPageContainer: MainPageContainerElement|null;
  let testAccountManagerBrowserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(async () => {
    // Setup fake accounts. There must be a device account available for the
    // Accounts menu item in <os-settings-menu>.
    testAccountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        testAccountManagerBrowserProxy);

    // Create only one element instance for the entire suite since this element
    // is large and expensive to render to the DOM.
    clearBody();
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    settingsMain = ui.shadowRoot!.querySelector('os-settings-main');
    assert(settingsMain);

    mainPageContainer =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assert(mainPageContainer);

    const idleRender =
        mainPageContainer.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  });

  suiteTeardown(() => {
    ui.remove();
  });

  teardown(() => {
    testAccountManagerBrowserProxy.reset();
  });

  test('Update required end of life banner visibility', () => {
    flush();
    assert(mainPageContainer);
    assertEquals(
        null,
        mainPageContainer.shadowRoot!.querySelector(
            '#updateRequiredEolBanner'));

    mainPageContainer!.set('showUpdateRequiredEolBanner_', true);
    flush();
    assertTrue(!!mainPageContainer.shadowRoot!.querySelector(
        '#updateRequiredEolBanner'));
  });

  test('Update required end of life banner close button click', () => {
    assert(mainPageContainer);
    mainPageContainer.set('showUpdateRequiredEolBanner_', true);
    flush();
    const banner = mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
        '#updateRequiredEolBanner');
    assertTrue(!!banner);

    const closeButton =
        mainPageContainer.shadowRoot!.querySelector<HTMLElement>(
            '#closeUpdateRequiredEol');
    assert(closeButton);
    closeButton.click();
    flush();
    assertEquals('none', banner.style.display);
  });

  test('clicking icon closes drawer', async () => {
    flush();
    const drawer = ui.shadowRoot!.querySelector<CrDrawerElement>('#drawer');
    assert(drawer);
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);

    // Clicking the drawer icon closes the drawer.
    ui.shadowRoot!.querySelector<HTMLElement>('#drawerIcon')!.click();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
    assertTrue(drawer.wasCanceled());
  });
});
