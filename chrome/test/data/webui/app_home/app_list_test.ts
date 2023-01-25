// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://apps/app_list.js';
import 'chrome://apps/app_item.js';

import {AppInfo, PageRemote, RunOnOsLoginMode} from 'chrome://apps/app_home.mojom-webui.js';
import {AppListElement} from 'chrome://apps/app_list.js';
import {BrowserProxy} from 'chrome://apps/browser_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestAppHomeBrowserProxy} from './test_app_home_browser_proxy.js';

interface AppList {
  appList: AppInfo[];
}

suite('AppListTest', () => {
  let appListElement: AppListElement;
  let apps: AppList;
  let testBrowserProxy: TestAppHomeBrowserProxy;
  let callbackRouterRemote: PageRemote;
  let testAppInfo: AppInfo;

  setup(async () => {
    apps = {
      appList: [
        {
          id: 'ahfgeienlihckogmohjhadlkjgocpleb',
          startUrl: {url: 'https://test.google.com/testapp1'},
          name: 'Test App 1',
          iconUrl: {
            url:
                'chrome://extension-icon/ahfgeienlihckogmohjhadlkjgocpleb/128/1',
          },
          mayShowRunOnOsLoginMode: true,
          mayToggleRunOnOsLoginMode: false,
          runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
          isLocallyInstalled: true,
          openInWindow: false,
        },
        {
          id: 'ahfgeienlihckogmotestdlkjgocpleb',
          startUrl: {url: 'https://test.google.com/testapp2'},
          name: 'Test App 2',
          iconUrl: {
            url:
                'chrome://extension-icon/ahfgeienlihckogmotestdlkjgocpleb/128/1',
          },
          mayShowRunOnOsLoginMode: false,
          mayToggleRunOnOsLoginMode: false,
          runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
          isLocallyInstalled: false,
          openInWindow: false,
        },
      ],
    };

    testAppInfo = {
      id: 'mmfbcljfglbokpmkimbfghdkjmjhdgbg',
      startUrl: {url: 'https://test.google.com/testapp3'},
      name: 'Test App 3',
      iconUrl: {
        url: 'chrome://extension-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/128/1',
      },
      mayShowRunOnOsLoginMode: false,
      mayToggleRunOnOsLoginMode: false,
      runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
      isLocallyInstalled: true,
      openInWindow: false,
    };
    testBrowserProxy = new TestAppHomeBrowserProxy(apps);
    callbackRouterRemote = testBrowserProxy.callbackRouterRemote;
    BrowserProxy.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    appListElement = document.createElement('app-list');
    document.body.appendChild(appListElement);
    await waitAfterNextRender(appListElement);
  });

  test('app list present', () => {
    assertTrue(!!appListElement);

    const appItems = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertTrue(!!appItems);
    assertEquals(apps.appList.length, appItems.length);

    assertEquals(
        appItems[0]!.shadowRoot!.querySelector('.text-container')!.textContent,
        apps.appList[0]!.name);
    assertEquals(
        appItems[0]!.shadowRoot!
            .querySelector<HTMLImageElement>('.icon-container img')!.src,
        apps.appList[0]!.iconUrl.url);

    assertEquals(
        appItems[1]!.shadowRoot!.querySelector('.text-container')!.textContent,
        apps.appList[1]!.name);
    assertEquals(
        appItems[1]!.shadowRoot!
            .querySelector<HTMLImageElement>('.icon-container img')!.src,
        apps.appList[1]!.iconUrl.url + '?grayscale=true');
  });

  test('add/remove app', async () => {
    // Test adding an app.
    callbackRouterRemote.addApp(testAppInfo);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    let appItemList =
        Array.from(appListElement.shadowRoot!.querySelectorAll('app-item'));
    assertTrue(!!appItemList.find(
        appItem => appItem.shadowRoot!.querySelector(
                                          '.text-container')!.textContent ===
            testAppInfo.name));

    // Test removing an app
    callbackRouterRemote.removeApp(testAppInfo);
    await callbackRouterRemote.$.flushForTesting();
    flush();
    appItemList =
        Array.from(appListElement.shadowRoot!.querySelectorAll('app-item'));
    assertFalse(!!appItemList.find(
        appItem => appItem.shadowRoot!.querySelector(
                                          '.text-container')!.textContent ===
            testAppInfo.name));
  });

  test('context menu locally installed', () => {
    // Get the first app item.
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    const contextMenu =
        appListElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertTrue(contextMenu.hidden);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(contextMenu.hidden);

    assertTrue(apps.appList.length >= 1);
    const appInfo = apps.appList[0]!;

    const openInWindow =
        contextMenu.querySelector<HTMLElement>('#open-in-window');
    assertTrue(!!openInWindow);
    assertEquals(openInWindow.hidden, !appInfo.isLocallyInstalled);
    assertEquals(
        openInWindow.querySelector('cr-checkbox')!.checked,
        appInfo.openInWindow);

    const launchOnStartup =
        contextMenu.querySelector<HTMLElement>('#launch-on-startup');
    assertTrue(!!launchOnStartup);
    assertEquals(launchOnStartup.hidden, !appInfo.mayShowRunOnOsLoginMode);

    assertEquals(
        launchOnStartup.querySelector('cr-checkbox')!.checked,
        (appInfo.runOnOsLoginMode !== RunOnOsLoginMode.kNotRun));
    assertEquals(
        launchOnStartup.querySelector('cr-checkbox')!.disabled,
        !appInfo.mayToggleRunOnOsLoginMode);

    assertTrue(!!contextMenu.querySelector('#create-shortcut'));
    assertTrue(!!contextMenu.querySelector('#uninstall'));
    assertTrue(!!contextMenu.querySelector('#app-settings'));
    assertTrue(!!contextMenu.querySelector('#install-locally'));

    assertFalse(
        contextMenu.querySelector<HTMLElement>('#create-shortcut')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#app-settings')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#install-locally')!.hidden);
  });

  test('context menu not locally installed', () => {
    // Get the second app item that's not locally installed.
    const appList = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertEquals(appList.length, 2);
    const appItem = appList[1];
    assertTrue(!!appItem);

    const contextMenu =
        appListElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertTrue(contextMenu.hidden);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(contextMenu.hidden);

    assertTrue(
        contextMenu.querySelector<HTMLElement>('#open-in-window')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#launch-on-startup')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#create-shortcut')!.hidden);
    assertTrue(contextMenu.querySelector<HTMLElement>('#app-settings')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#install-locally')!.hidden);
  });

  test('toggle open in window', () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    assertTrue(apps.appList.length >= 1);
    const appInfo = apps.appList[0]!;
    const openInWindow = appListElement.shadowRoot!.querySelector<HTMLElement>(
        '#open-in-window');
    assertTrue(!!openInWindow);
    const checkbox = openInWindow.querySelector('cr-checkbox');
    assertTrue(!!checkbox);

    assertFalse(checkbox.checked);
    assertFalse(appInfo.openInWindow);
    openInWindow.click();
    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(checkbox.checked);
    assertTrue(appInfo.openInWindow);
    openInWindow.click();
    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(checkbox.checked);
    assertFalse(appInfo.openInWindow);

    // Clicking the checkbox should have the same effect as click the parent
    // menu item.
    checkbox.click();
    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(checkbox.checked);
    assertTrue(appInfo.openInWindow);
  });

  test('toggle launch on startup', () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    assertTrue(apps.appList.length > 0);
    const appInfo = apps.appList[0]!;
    const launchOnStartup =
        appListElement.shadowRoot!.querySelector<HTMLElement>(
            '#launch-on-startup');
    assertTrue(!!launchOnStartup);
    const checkbox = launchOnStartup.querySelector('cr-checkbox');
    assertTrue(!!checkbox);

    assertFalse(checkbox.checked);
    assertEquals(appInfo.runOnOsLoginMode, RunOnOsLoginMode.kNotRun);
    launchOnStartup.click();
    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(checkbox.checked);
    assertEquals(appInfo.runOnOsLoginMode, RunOnOsLoginMode.kWindowed);
    launchOnStartup.click();
    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(checkbox.checked);
    assertEquals(appInfo.runOnOsLoginMode, RunOnOsLoginMode.kNotRun);

    // Clicking the checkbox should have the same effect as click the parent
    // menu item.
    checkbox.click();
    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(checkbox.checked);
    assertEquals(appInfo.runOnOsLoginMode, RunOnOsLoginMode.kWindowed);
  });

  test('click uninstall', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const uninstall =
        appListElement.shadowRoot!.querySelector<HTMLElement>('#uninstall');
    assertTrue(!!uninstall);

    uninstall.click();
    await testBrowserProxy.fakeHandler.whenCalled('uninstallApp')
        .then((appId: string) => assertEquals(appId, apps.appList[0]!.id));
  });

  test('click app settings', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const appSettings =
        appListElement.shadowRoot!.querySelector<HTMLElement>('#app-settings');
    assertTrue(!!appSettings);

    appSettings.click();
    await testBrowserProxy.fakeHandler.whenCalled('showAppSettings')
        .then((appId: string) => assertEquals(appId, apps.appList[0]!.id));
  });

  test('click create shortcut', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const createShortcut =
        appListElement.shadowRoot!.querySelector<HTMLElement>(
            '#create-shortcut');
    assertTrue(!!createShortcut);

    createShortcut.click();
    await testBrowserProxy.fakeHandler.whenCalled('createAppShortcut')
        .then((appId: string) => assertEquals(appId, apps.appList[0]!.id));
  });

  test('click install locally', async () => {
    const appItem = appListElement.shadowRoot!.querySelectorAll('app-item')[1];
    assertTrue(!!appItem);

    assertEquals(
        appItem.shadowRoot!
            .querySelector<HTMLImageElement>('.icon-container img')!.src,
        apps.appList[1]!.iconUrl.url + '?grayscale=true');

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const contextMenu =
        appListElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);

    assertTrue(
        contextMenu.querySelector<HTMLElement>('#open-in-window')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#create-shortcut')!.hidden);
    assertTrue(contextMenu.querySelector<HTMLElement>('#app-settings')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);

    const installLocally =
        appListElement.shadowRoot!.querySelector<HTMLElement>(
            '#install-locally');
    assertTrue(!!installLocally);
    assertFalse(installLocally.hidden);

    installLocally.click();
    await testBrowserProxy.fakeHandler.whenCalled('installAppLocally')
        .then((appId: string) => assertEquals(appId, apps.appList[1]!.id));

    await callbackRouterRemote.$.flushForTesting();
    flush();
    assertEquals(
        appItem.shadowRoot!
            .querySelector<HTMLImageElement>('.icon-container img')!.src,
        apps.appList[1]!.iconUrl.url);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    assertFalse(
        contextMenu.querySelector<HTMLElement>('#open-in-window')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#create-shortcut')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#app-settings')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#install-locally')!.hidden);
  });

});
