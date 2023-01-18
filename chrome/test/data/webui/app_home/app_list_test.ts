// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://apps/app_list.js';
import 'chrome://apps/app_item.js';

import {AppInfo, PageRemote} from 'chrome://apps/app_home.mojom-webui.js';
import {AppListElement} from 'chrome://apps/app_list.js';
import {BrowserProxy} from 'chrome://apps/browser_proxy.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
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
          runOnOsLoginMode: 0 /*kNotRun*/,
          mayShowOpenInWindow: true,
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
          runOnOsLoginMode: 0 /*kNotRun*/,
          mayShowOpenInWindow: false,
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
      runOnOsLoginMode: 0 /*kNotRun*/,
      mayShowOpenInWindow: false,
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
        apps.appList[1]!.iconUrl.url);
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

  test('context menu', () => {
    // Get the first app item.
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    const contextMenu =
        appListElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertTrue(contextMenu!.hidden);

    appItem!.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(contextMenu!.hidden);

    const appInfo = apps.appList[0]!;

    const openInWindow =
        contextMenu!.querySelector<HTMLElement>('#open-in-window');
    assertTrue(!!openInWindow);
    assertEquals(openInWindow!.hidden, !appInfo.mayShowOpenInWindow);
    assertEquals(
        openInWindow!.querySelector<CrCheckboxElement>('cr-checkbox')!.checked,
        appInfo.openInWindow);

    const launchOnStartup =
        contextMenu!.querySelector<HTMLElement>('#launch-on-startup');
    assertTrue(!!launchOnStartup);
    assertEquals(launchOnStartup!.hidden, !appInfo.mayShowRunOnOsLoginMode);

    assertEquals(
        launchOnStartup!.querySelector<CrCheckboxElement>(
                            'cr-checkbox')!.checked,
        (appInfo.runOnOsLoginMode !== 0));
    assertEquals(
        launchOnStartup!.querySelector<CrCheckboxElement>(
                            'cr-checkbox')!.disabled,
        !appInfo.mayToggleRunOnOsLoginMode);

    assertTrue(!!contextMenu!.querySelector('#create-shortcut'));
    assertTrue(!!contextMenu!.querySelector('#uninstall'));
    assertTrue(!!contextMenu!.querySelector('#app-settings'));
  });

  test('toggle open in window', () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem!.dispatchEvent(new CustomEvent('contextmenu'));

    const appInfo = apps.appList[0]!;
    const openInWindow = appListElement.shadowRoot!.querySelector<HTMLElement>(
        '#open-in-window');
    assertTrue(!!openInWindow);

    assertFalse(
        openInWindow!.querySelector<CrCheckboxElement>('cr-checkbox')!.checked);
    assertFalse(appInfo.openInWindow);
    openInWindow!.click();
    appItem!.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(
        openInWindow!.querySelector<CrCheckboxElement>('cr-checkbox')!.checked);
    assertTrue(appInfo.openInWindow);
    openInWindow!.click();
    appItem!.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(
        openInWindow!.querySelector<CrCheckboxElement>('cr-checkbox')!.checked);
    assertFalse(appInfo.openInWindow);
  });

});
