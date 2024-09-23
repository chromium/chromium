// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAppManagementPageElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {isHiddenByDomIf, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<settings-app-management-page>', () => {
  let appManagementPage: SettingsAppManagementPageElement;
  let fakeHandler: FakePageHandler;

  function getAppList(): HTMLElement {
    const page =
        appManagementPage.shadowRoot!.querySelector('app-management-main-view');
    assertTrue(!!page);
    const element = page.shadowRoot!.querySelector<HTMLElement>('#appList');
    assertTrue(!!element);
    return element;
  }

  function getAppListChildren(): number {
    return getAppList().childElementCount -
        1;  // Ignore the dom-repeat element.
  }

  function getNoAppsFoundLabel(): HTMLElement {
    const page =
        appManagementPage.shadowRoot!.querySelector('app-management-main-view');
    assertTrue(!!page);
    const noAppsLabel =
        page.shadowRoot!.querySelector<HTMLElement>('#noAppsLabel');
    assertTrue(!!noAppsLabel);
    return noAppsLabel;
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();
    appManagementPage = document.createElement('settings-app-management-page');
    assertTrue(!!appManagementPage);
    replaceBody(appManagementPage);
    await flushTasks();
  });

  teardown(() => {
    appManagementPage.remove();
  });

  test('loads', async () => {
    // Check that the browser responds to the getApps() message.
    await AppManagementBrowserProxy.getInstance().handler.getApps();
  });

  test('App list renders on page change', async () => {
    await fakeHandler.addApp();
    assertEquals(1, getAppListChildren());
    await fakeHandler.addApp();
    assertEquals(2, getAppListChildren());
  });

  test('No Apps Found Label', async () => {
    assertEquals(0, getAppListChildren());
    assertFalse(isHiddenByDomIf(getNoAppsFoundLabel()));

    const app = await fakeHandler.addApp();
    assertEquals(1, getAppListChildren());
    assertTrue(isHiddenByDomIf(getNoAppsFoundLabel()));

    fakeHandler.uninstall(app.id);
    await flushTasks();
    assertEquals(0, getAppListChildren());
    assertFalse(isHiddenByDomIf(getNoAppsFoundLabel()));
  });

  test('App list filters when searching', async () => {
    await fakeHandler.addApp('', {title: 'slides'});
    await fakeHandler.addApp('', {title: 'calculator'});
    const sheets = await fakeHandler.addApp('', {title: 'sheets'});
    assertEquals(3, getAppListChildren());

    appManagementPage.searchTerm = 's';
    await flushTasks();
    assertEquals(2, getAppListChildren());

    fakeHandler.uninstall(sheets.id);
    await flushTasks();
    assertEquals(1, getAppListChildren());

    appManagementPage.searchTerm = 'ss';
    await flushTasks();
    assertEquals(0, getAppListChildren());
    assertFalse(isHiddenByDomIf(getNoAppsFoundLabel()));

    appManagementPage.searchTerm = '';
    await flushTasks();
    assertEquals(2, getAppListChildren());
  });
});
