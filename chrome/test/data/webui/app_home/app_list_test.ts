// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://apps/app_list.js';
import 'chrome://apps/app_item.js';
import 'chrome://apps/deprecated_apps_link.js';

import type {AppInfo, PageRemote} from 'chrome://apps/app_home.mojom-webui.js';
import {RunOnOsLoginMode} from 'chrome://apps/app_home.mojom-webui.js';
import type {AppHomeEmptyPageElement} from 'chrome://apps/app_home_empty_page.js';
import {AppHomeUserAction} from 'chrome://apps/app_home_utils.js';
import type {AppListElement} from 'chrome://apps/app_list.js';
import {BrowserProxy} from 'chrome://apps/browser_proxy.js';
import type {DeprecatedAppsLinkElement} from 'chrome://apps/deprecated_apps_link.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestAppHomeBrowserProxy} from './test_app_home_browser_proxy.js';

interface AppList {
  appList: AppInfo[];
}

/**
 * A mock to intercept User Action logging calls and verify how many times they
 * were called.
 */
class MetricsPrivateMock {
  userActionMap: Map<string, number> = new Map();

  getUserActionCount(metricName: string): number {
    return this.userActionMap.get(metricName) || 0;
  }

  recordUserAction(metricName: string) {
    this.userActionMap.set(metricName, this.getUserActionCount(metricName) + 1);
  }
}

suite('AppListTest', () => {
  let appListElement: AppListElement;
  let apps: AppList;
  let testBrowserProxy: TestAppHomeBrowserProxy;
  let callbackRouterRemote: PageRemote;
  let testAppInfo: AppInfo;
  let deprecatedAppInfo: AppInfo;
  let metricsPrivateMock: MetricsPrivateMock;

  setup(async () => {
    apps = {
      appList: [
        {
          id: 'ahfgeienlihckogmohjhadlkjgocpleb',
          startUrl: {url: 'https://test.google.com/testapp1'},
          name: 'Test App 1',
          iconUrl: {
            url: 'chrome://app-icon/ahfgeienlihckogmohjhadlkjgocpleb/128/1',
          },
          mayShowRunOnOsLoginMode: true,
          mayToggleRunOnOsLoginMode: true,
          runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
          isLocallyInstalled: true,
          mayUninstall: true,
          openInWindow: false,
          isDeprecatedApp: false,
          storePageUrl: null,
        },
        {
          id: 'ahfgeienlihckogmotestdlkjgocpleb',
          startUrl: {url: 'https://test.google.com/testapp2'},
          name: 'Test App 2',
          iconUrl: {
            url: 'chrome://app-icon/ahfgeienlihckogmotestdlkjgocpleb/128/1',
          },
          mayShowRunOnOsLoginMode: false,
          mayToggleRunOnOsLoginMode: false,
          runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
          isLocallyInstalled: false,
          mayUninstall: false,
          openInWindow: false,
          isDeprecatedApp: false,
          storePageUrl: null,
        },
      ],
    };

    testAppInfo = {
      id: 'mmfbcljfglbokpmkimbfghdkjmjhdgbg',
      startUrl: {url: 'https://test.google.com/testapp3'},
      name: 'A Test App 3',
      iconUrl: {
        url: 'chrome://app-icon/mmfbcljfglbokpmkimbfghdkjmjhdgbg/128/1',
      },
      mayShowRunOnOsLoginMode: false,
      mayToggleRunOnOsLoginMode: false,
      runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
      isLocallyInstalled: true,
      openInWindow: false,
      mayUninstall: true,
      isDeprecatedApp: false,
      storePageUrl: null,
    };
    deprecatedAppInfo = {
      id: 'mplpmdejoamenolpcojgegminhcnmibo',
      startUrl: {url: 'https://test.google.com/deprecated_app'},
      name: 'Deprecated App',
      iconUrl: {
        url: 'chrome://extension-icon/mplpmdejoamenolpcojgegminhcnmibo/128/1',
      },
      mayShowRunOnOsLoginMode: false,
      mayToggleRunOnOsLoginMode: false,
      runOnOsLoginMode: RunOnOsLoginMode.kNotRun,
      isLocallyInstalled: true,
      openInWindow: true,
      mayUninstall: true,
      isDeprecatedApp: true,
      storePageUrl: {
        url: '',
      },
    };
    metricsPrivateMock = new MetricsPrivateMock();
    chrome.metricsPrivate =
        metricsPrivateMock as unknown as typeof chrome.metricsPrivate;
    testBrowserProxy = new TestAppHomeBrowserProxy(apps);
    callbackRouterRemote = testBrowserProxy.callbackRouterRemote;
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    appListElement = document.createElement('app-list');
    document.body.appendChild(appListElement);
    await microtasksFinished();
  });

  test('app list present', () => {
    assertTrue(!!appListElement);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(AppHomeUserAction.APP_HOME_INIT));

    const appItems = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertTrue(!!appItems);
    assertEquals(apps.appList.length, appItems.length);

    assertEquals(
        appItems[0]!.shadowRoot!.querySelector('#textContainer')!.textContent,
        apps.appList[0]!.name);
    assertEquals(
        appItems[0]!.shadowRoot!.querySelector<HTMLImageElement>(
                                    '#iconImage')!.src,
        apps.appList[0]!.iconUrl.url);

    assertEquals(
        appItems[1]!.shadowRoot!.querySelector('#textContainer')!.textContent,
        apps.appList[1]!.name);
    assertEquals(
        appItems[1]!.shadowRoot!.querySelector<HTMLImageElement>(
                                    '#iconImage')!.src,
        apps.appList[1]!.iconUrl.url + '?grayscale=true');
  });

  test('add/remove app', async () => {
    // Test adding an app.
    callbackRouterRemote.addApp(testAppInfo);
    await callbackRouterRemote.$.flushForTesting();
    let appItemList =
        Array.from(appListElement.shadowRoot!.querySelectorAll('app-item'));
    assertTrue(
        appItemList[0]!.shadowRoot!.querySelector(
                                       '#textContainer')!.textContent ===
        testAppInfo.name);

    // Test removing an app
    callbackRouterRemote.removeApp(testAppInfo);
    await callbackRouterRemote.$.flushForTesting();
    appItemList =
        Array.from(appListElement.shadowRoot!.querySelectorAll('app-item'));
    assertFalse(!!appItemList.find(
        appItem =>
            appItem.shadowRoot!.querySelector('#textContainer')!.textContent ===
            testAppInfo.name));
  });

  test('context menu locally installed', () => {
    // Get the first app item.
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertFalse(contextMenu.open);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(contextMenu.open);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.CONTEXT_MENU_TRIGGERED));

    assertTrue(apps.appList.length >= 1);
    const appInfo = apps.appList[0]!;

    const openInWindow =
        contextMenu.querySelector<CrCheckboxElement>('#openInWindow');
    assertTrue(!!openInWindow);
    assertEquals(openInWindow.hidden, !appInfo.isLocallyInstalled);
    assertEquals(openInWindow.checked, appInfo.openInWindow);

    const launchOnStartup =
        contextMenu.querySelector<CrCheckboxElement>('#launchOnStartup');
    assertTrue(!!launchOnStartup);
    assertEquals(launchOnStartup.hidden, !appInfo.mayShowRunOnOsLoginMode);

    assertEquals(
        launchOnStartup.checked,
        (appInfo.runOnOsLoginMode !== RunOnOsLoginMode.kNotRun));
    assertEquals(launchOnStartup.disabled, !appInfo.mayToggleRunOnOsLoginMode);

    assertTrue(!!contextMenu.querySelector('#createShortcut'));
    assertTrue(!!contextMenu.querySelector('#uninstall'));
    assertTrue(!!contextMenu.querySelector('#appSettings'));
    assertTrue(!!contextMenu.querySelector('#installLocally'));

    assertFalse(
        contextMenu.querySelector<HTMLElement>('#createShortcut')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLButtonElement>('#uninstall')!.disabled);
    assertFalse(contextMenu.querySelector<HTMLElement>('#appSettings')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#installLocally')!.hidden);
  });

  test('context menu not locally installed', () => {
    // Get the second app item that's not locally installed.
    const appList = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertEquals(appList.length, 2);
    const appItem = appList[1];
    assertTrue(!!appItem);

    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertFalse(contextMenu.open);
    assertEquals(
        0,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.CONTEXT_MENU_TRIGGERED));

    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(contextMenu.open);

    assertTrue(contextMenu.querySelector<HTMLElement>('#openInWindow')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#launchOnStartup')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#createShortcut')!.hidden);
    assertTrue(contextMenu.querySelector<HTMLElement>('#appSettings')!.hidden);
    assertTrue(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#removeFromChrome')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLButtonElement>(
                       '#removeFromChrome')!.disabled);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#installLocally')!.hidden);
  });

  test('toggle open in window', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    assertTrue(apps.appList.length >= 1);
    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    const openInWindow =
        contextMenu.querySelector<CrCheckboxElement>('#openInWindow');
    assertTrue(!!openInWindow);
    assertFalse(openInWindow.checked);
    assertFalse(apps.appList[0]!.openInWindow);

    // Clicking on the open in window context menu option to toggle
    // on or off.
    openInWindow.click();
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(openInWindow.checked);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.OPEN_IN_WINDOW_CHECKED));
    assertTrue(apps.appList[0]!.openInWindow);

    openInWindow.click();
    await callbackRouterRemote.$.flushForTesting();
    assertFalse(openInWindow.checked);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.OPEN_IN_WINDOW_UNCHECKED));
    assertFalse(apps.appList[0]!.openInWindow);
  });

  test('toggle launch on startup', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    assertTrue(apps.appList.length >= 1);
    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    const launchOnStartup =
        contextMenu.querySelector<CrCheckboxElement>('#launchOnStartup');
    assertTrue(!!launchOnStartup);
    assertFalse(launchOnStartup.checked);
    assertEquals(apps.appList[0]!.runOnOsLoginMode, RunOnOsLoginMode.kNotRun);

    // Clicking on the launch on startup context menu option to toggle
    // on or off.
    launchOnStartup.click();
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(launchOnStartup.checked);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.LAUNCH_AT_STARTUP_CHECKED));
    assertEquals(apps.appList[0]!.runOnOsLoginMode, RunOnOsLoginMode.kWindowed);

    launchOnStartup.click();
    await callbackRouterRemote.$.flushForTesting();
    assertFalse(launchOnStartup.checked);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.LAUNCH_AT_STARTUP_UNCHECKED));
    assertEquals(apps.appList[0]!.runOnOsLoginMode, RunOnOsLoginMode.kNotRun);
  });

  test('toggle launch on startup disabled', async () => {
    const appList = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertEquals(appList.length, 2);
    const appItem = appList[1];
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    const launchOnStartup =
        contextMenu.querySelector<CrCheckboxElement>('#launchOnStartup');
    assertTrue(!!launchOnStartup);
    assertFalse(launchOnStartup.checked);
    assertEquals(apps.appList[1]!.runOnOsLoginMode, RunOnOsLoginMode.kNotRun);

    // Clicking on the launch on startup context menu option should not toggle
    // if mayToggleRunOnOsLoginMode is false. The user actions should also
    // not get fired.
    launchOnStartup.click();
    await callbackRouterRemote.$.flushForTesting();
    assertFalse(launchOnStartup.checked);
    assertEquals(
        0,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.LAUNCH_AT_STARTUP_CHECKED));
    assertEquals(apps.appList[1]!.runOnOsLoginMode, RunOnOsLoginMode.kNotRun);
  });

  test('click uninstall', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const uninstall =
        appItem.shadowRoot!.querySelector<HTMLElement>('#uninstall');
    assertTrue(!!uninstall);

    uninstall.click();
    await testBrowserProxy.fakeHandler.whenCalled('uninstallApp')
        .then((appId: string) => assertEquals(appId, apps.appList[0]!.id));
    assertEquals(
        1, metricsPrivateMock.getUserActionCount(AppHomeUserAction.UNINSTALL));
  });

  test('click app settings', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const appSettings =
        appItem.shadowRoot!.querySelector<HTMLElement>('#appSettings');
    assertTrue(!!appSettings);

    appSettings.click();
    await testBrowserProxy.fakeHandler.whenCalled('showAppSettings')
        .then((appId: string) => assertEquals(appId, apps.appList[0]!.id));
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.OPEN_APP_SETTINGS));
  });

  test('click create shortcut', async () => {
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const createShortcut =
        appItem.shadowRoot!.querySelector<HTMLElement>('#createShortcut');
    assertTrue(!!createShortcut);

    createShortcut.click();
    await testBrowserProxy.fakeHandler.whenCalled('createAppShortcut')
        .then((appId: string) => assertEquals(appId, apps.appList[0]!.id));
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.CREATE_SHORTCUT));
  });

  test('click install locally', async () => {
    const appItem = appListElement.shadowRoot!.querySelectorAll('app-item')[1];
    assertTrue(!!appItem);

    assertEquals(
        appItem.shadowRoot!.querySelector<HTMLImageElement>('#iconImage')!.src,
        apps.appList[1]!.iconUrl.url + '?grayscale=true');

    assertEquals(appItem.ariaLabel, 'Test App 2 (not locally installed)');

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);

    assertTrue(contextMenu.querySelector<HTMLElement>('#openInWindow')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#createShortcut')!.hidden);
    assertTrue(contextMenu.querySelector<HTMLElement>('#appSettings')!.hidden);
    assertTrue(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#removeFromChrome')!.hidden);

    const installLocally =
        appItem.shadowRoot!.querySelector<HTMLElement>('#installLocally');
    assertTrue(!!installLocally);
    assertFalse(installLocally.hidden);

    installLocally.click();
    await testBrowserProxy.fakeHandler.whenCalled('installAppLocally')
        .then((appId: string) => assertEquals(appId, apps.appList[1]!.id));

    await callbackRouterRemote.$.flushForTesting();
    assertEquals(
        appItem.shadowRoot!.querySelector<HTMLImageElement>('#iconImage')!.src,
        apps.appList[1]!.iconUrl.url);

    assertEquals(appItem.ariaLabel, 'Test App 2');

    appItem.dispatchEvent(new CustomEvent('contextmenu'));

    assertFalse(
        contextMenu.querySelector<HTMLElement>('#openInWindow')!.hidden);
    assertFalse(
        contextMenu.querySelector<HTMLElement>('#createShortcut')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#appSettings')!.hidden);
    assertFalse(contextMenu.querySelector<HTMLElement>('#uninstall')!.hidden);
    assertTrue(
        contextMenu.querySelector<HTMLElement>('#installLocally')!.hidden);
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.INSTALL_APP_LOCALLY));
  });

  test('click launch launches app', async () => {
    const appItem = appListElement.shadowRoot!.querySelectorAll('app-item')[1];
    assertTrue(!!appItem);

    const mouseEvent: MouseEvent = new MouseEvent('click', {
      button: 0,
      altKey: false,
      ctrlKey: false,
      metaKey: false,
      shiftKey: false,
    });

    appItem.dispatchEvent(mouseEvent);
    const [appId, clickEvent] =
        await testBrowserProxy.fakeHandler.whenCalled('launchApp');
    assertEquals(appId, apps.appList[1]!.id);
    assertEquals(clickEvent.button, mouseEvent.button);
    assertEquals(clickEvent.altKey, mouseEvent.altKey);
    assertEquals(clickEvent.ctrlKey, mouseEvent.ctrlKey);
    assertEquals(clickEvent.metaKey, mouseEvent.metaKey);
    assertEquals(clickEvent.shiftKey, mouseEvent.shiftKey);

    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.LAUNCH_WEB_APP));
  });

  test(
      'context menu right click opens corresponding menu for different app',
      () => {
        assertTrue(!!appListElement);

        const appItems =
            appListElement.shadowRoot!.querySelectorAll('app-item');
        assertTrue(!!appItems);
        assertEquals(apps.appList.length, appItems.length);
        assertTrue(!!appItems[0]);
        assertTrue(!!appItems[1]);

        appItems[0].dispatchEvent(new CustomEvent('contextmenu'));
        const contextMenu1 =
            appItems[0].shadowRoot!.querySelector('cr-action-menu');
        const contextMenu2 =
            appItems[1].shadowRoot!.querySelector('cr-action-menu');
        assertTrue(!!contextMenu1);
        assertTrue(!!contextMenu2);
        assertTrue(contextMenu1.open);
        assertFalse(contextMenu2.open);

        // Simulate right click on 2nd app such that the context menu for the
        // 2nd app shows up, and the context menu for the 1st app is hidden.
        appItems[1].dispatchEvent(new CustomEvent('contextmenu'));
        assertFalse(contextMenu1.open);
        assertTrue(contextMenu2.open);
      });

  test('context menu close on right click on document', () => {
    assertTrue(!!appListElement);

    const appItems = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertTrue(!!appItems);
    assertEquals(apps.appList.length, appItems.length);
    assertTrue(!!appItems[0]);

    appItems[0].dispatchEvent(new CustomEvent('contextmenu'));
    const contextMenu = appItems[0].shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertTrue(contextMenu.open);

    // Simulate right click on document such that the context menu is hidden
    // again.
    document.dispatchEvent(new CustomEvent('contextmenu'));
    assertFalse(contextMenu.open);
  });

  test('navigate with arrow keys', async () => {
    appListElement.shadowRoot!.getElementById(
                                  'container')!.style.gridTemplateColumns =
        'repeat(2, max(100% / 2, 112px))';
    callbackRouterRemote.addApp(testAppInfo);
    await callbackRouterRemote.$.flushForTesting();
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    assertEquals(
        apps.appList[1]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        apps.appList[1]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        apps.appList[2]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        apps.appList[2]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);
  });

  test('enter when focused on app launches app', async () => {
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);

    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    const [appId, clickEvent] =
        await testBrowserProxy.fakeHandler.whenCalled('launchApp');
    assertEquals(appId, apps.appList[0]!.id);
    assertEquals(clickEvent, null);
  });

  test('No deprecated apps means no deprecated app ux', async () => {
    const deprecatedAppsLink: DeprecatedAppsLinkElement =
        document.createElement('deprecated-apps-link');
    document.body.appendChild(deprecatedAppsLink);
    await microtasksFinished();

    assertTrue(!!deprecatedAppsLink);
    const linkContainer =
        deprecatedAppsLink.shadowRoot!.querySelector<HTMLImageElement>(
            '#container');
    assertNull(linkContainer, 'Deprecation link is not hidden.');

    const appItems = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertTrue(!!appItems, 'No apps.');

    appItems.forEach((item) => {
      const deprecatedIcon =
          item!.shadowRoot!.querySelector<HTMLImageElement>('#deprecatedIcon')!;
      assertTrue(
          deprecatedIcon.hidden,
          'Non-deprecated app should not have deprecation icon');
    });
  });

  test('Deprecated link', async () => {
    testBrowserProxy.fakeHandler.addAppToList(deprecatedAppInfo);

    const deprecatedAppsLink: DeprecatedAppsLinkElement =
        document.createElement('deprecated-apps-link');
    document.body.appendChild(deprecatedAppsLink);
    await microtasksFinished();
    assertTrue(!!deprecatedAppsLink);
    const linkContainer =
        deprecatedAppsLink.shadowRoot!.querySelector<HTMLImageElement>(
            '#container');
    assertTrue(!!linkContainer);
  });

  test('Deprecated app icon', async () => {
    // Test adding an app.
    callbackRouterRemote.addApp(deprecatedAppInfo);
    await callbackRouterRemote.$.flushForTesting();

    const appItems = appListElement.shadowRoot!.querySelectorAll('.item');
    assertTrue(!!appItems, 'No apps.');

    let found = false;
    appItems.forEach((item) => {
      const deprecatedIcon =
          item!.shadowRoot!.querySelector<HTMLImageElement>('#deprecatedIcon')!;
      if (item!.id === deprecatedAppInfo.id) {
        found = true;
        assertFalse(
            deprecatedIcon.hidden,
            'Deprecated app should have deprecated icon visible');
      } else {
        assertTrue(
            deprecatedIcon.hidden,
            'Non-deprecated app should not have deprecation icon');
      }
    });
    assertTrue(found, 'Deprecated item not found.');
  });

  test('Clicking deprecated app', async () => {
    // Test adding an app.
    callbackRouterRemote.addApp(deprecatedAppInfo);
    await callbackRouterRemote.$.flushForTesting();

    const appItem =
        appListElement.shadowRoot!.querySelector('#' + deprecatedAppInfo.id)!;
    assertTrue(!!appItem, 'No apps.');

    const mouseEvent: MouseEvent = new MouseEvent('click', {
      button: 0,
      altKey: false,
      ctrlKey: false,
      metaKey: false,
      shiftKey: false,
    });
    appItem.dispatchEvent(mouseEvent);

    await testBrowserProxy.fakeHandler.whenCalled('launchApp');
    assertEquals(
        1,
        metricsPrivateMock.getUserActionCount(
            AppHomeUserAction.LAUNCH_DEPRECATED_APP));
  });

  test('Clicking deprecation link calls handler', async () => {
    // Test adding an app.
    callbackRouterRemote.addApp(deprecatedAppInfo);
    testBrowserProxy.fakeHandler.addAppToList(deprecatedAppInfo);
    await callbackRouterRemote.$.flushForTesting();

    const deprecatedAppsLink: DeprecatedAppsLinkElement =
        document.createElement('deprecated-apps-link');
    document.body.appendChild(deprecatedAppsLink);
    await microtasksFinished();

    assertTrue(!!deprecatedAppsLink);
    const link = deprecatedAppsLink.shadowRoot!.querySelector<HTMLImageElement>(
        '#deprecated-apps-link')!;

    link.click();

    await testBrowserProxy.fakeHandler.whenCalled('launchDeprecatedAppDialog');
  });

  test('Empty app page', async () => {
    const emptyPage: AppHomeEmptyPageElement =
        document.createElement('app-home-empty-page');
    document.body.appendChild(emptyPage);
    await microtasksFinished();

    callbackRouterRemote.removeApp(apps.appList[0]!);
    callbackRouterRemote.removeApp(apps.appList[1]!);
    callbackRouterRemote.removeApp(deprecatedAppInfo);
    await callbackRouterRemote.$.flushForTesting();

    const appItems = appListElement.shadowRoot!.querySelectorAll('app-item');
    assertEquals(appItems.length, 0);

    const text: HTMLParagraphElement =
        emptyPage.shadowRoot!.querySelector<HTMLParagraphElement>('p')!;
    assertEquals(text.innerText, 'Web apps that you install appear here');

    const button: HTMLAnchorElement =
        emptyPage.shadowRoot!.querySelector<HTMLAnchorElement>('a')!;
    assertEquals(
        button.href, 'https://support.google.com/chrome?p=install_web_apps');
    assertEquals(button.innerText, 'Learn how to install web apps');
  });

  test('context menu not closed on checkbox click', async () => {
    // Test for crbug.com/1435592: Clicking the checkbox options on
    // the context menu does not close it.
    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    appItem.dispatchEvent(new CustomEvent('contextmenu'));
    assertTrue(apps.appList.length >= 1);

    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    const launchOnStartup =
        contextMenu.querySelector<CrCheckboxElement>('#launchOnStartup');
    assertTrue(!!launchOnStartup);
    assertFalse(launchOnStartup.checked);
    const openInWindow =
        contextMenu.querySelector<CrCheckboxElement>('#openInWindow');
    assertTrue(!!openInWindow);
    assertFalse(openInWindow.checked);

    // Launch on Startup check.
    launchOnStartup.click();
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(launchOnStartup.checked);
    assertFalse(contextMenu.hidden);

    // Open In Window check.
    openInWindow.click();
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(openInWindow.checked);
    assertFalse(contextMenu.hidden);
  });

  test('context menu opens on shift+f10 triggered on focused app', async () => {
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    assertEquals(
        apps.appList[0]!.id, appListElement.shadowRoot!.activeElement?.id);

    document.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'F10', shiftKey: true}));

    const appItem = appListElement.shadowRoot!.querySelector('app-item');
    assertTrue(!!appItem);

    const contextMenu = appItem.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!contextMenu);
    assertFalse(contextMenu.hidden);
  });
});
