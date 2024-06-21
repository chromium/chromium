// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAppParentalControlsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, Router, routes, setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppParentalControlsHandler} from './fake_app_parental_controls_handler.js';
import {createApp} from './test_utils.js';

suite('AppParentalControlsSubpage', () => {
  let page: SettingsAppParentalControlsSubpageElement;
  let handler: FakeAppParentalControlsHandler;

  async function createPage(): Promise<void> {
    Router.getInstance().navigateTo(routes.APP_PARENTAL_CONTROLS);
    page = new SettingsAppParentalControlsSubpageElement();
    // Set verified to true to indicate pin was accepted on the apps page.
    page.set('isVerified', true);
    document.body.appendChild(page);
    await flushTasks();
  }

  function getApps(): NodeListOf<HTMLElement> {
    const appList = page.shadowRoot!.querySelector('#appParentalControlsList');
    assertTrue(!!appList);
    assertTrue(isVisible(appList));
    return appList.querySelectorAll<HTMLElement>('block-app-item');
  }

  setup(() => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    page.remove();
  });

  test('App list is in alphabetical order', async () => {
    const appTitle1 = 'Files';
    const appTitle2 = 'Chrome';
    handler.addAppForTesting(createApp('file-id', appTitle1, true));
    handler.addAppForTesting(createApp('chrome-id', appTitle2, false));

    await createPage();

    const apps = getApps();
    assertEquals(2, apps.length);
    assertTrue(!!apps[0]);
    assertTrue(!!apps[1]);

    const title1 = apps[0].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title1);
    assertEquals(title1.innerText, appTitle2);

    const title2 = apps[1].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title2);
    assertEquals(title2.innerText, appTitle1);

    await flushTasks();

    const blockedAppsCount =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedAppsCount');
    assertTrue(!!blockedAppsCount);
    assertEquals(blockedAppsCount.innerText, '1 of 2 apps blocked');
  });

  test('Click toggle updates app blocked state', async () => {
    const app = createApp('file-id', 'Files', false);
    handler.addAppForTesting(app);

    await createPage();

    const apps = getApps();
    assertEquals(1, apps.length);

    const appElement = apps[0];
    assertTrue(!!appElement);
    const appToggle =
        appElement.shadowRoot!.querySelector<CrToggleElement>('.app-toggle');
    assertTrue(!!appToggle);
    assertTrue(appToggle.checked);

    appToggle.click();
    assertFalse(appToggle.checked);

    assertEquals(handler.getCallCount('updateApp'), 1);
    const args1 = handler.getArgs('updateApp')[0];
    assertEquals(2, args1.length);
    assertEquals(app.id, args1[0]);
    assertTrue(/*isBlocked*/ args1[1]);
    assertTrue(app.isBlocked);

    appToggle.click();
    assertTrue(appToggle.checked);

    assertEquals(handler.getCallCount('updateApp'), 2);
    const args2 = handler.getArgs('updateApp')[1];
    assertEquals(2, args2.length);
    assertEquals(app.id, args2[0]);
    assertFalse(/*isBlocked*/ args2[1]);
    assertFalse(app.isBlocked);
  });

  test('Installing and uninstalling app updates app list', async () => {
    const filesAppId = 'files-id';
    const filesAppTitle = 'Files';
    const app = createApp(filesAppId, filesAppTitle, false);
    handler.addAppForTesting(app);

    await createPage();

    const observer = handler.getObserverRemote();
    assertTrue(!!observer);

    assertEquals(1, getApps().length);

    const chromeAppId = 'chrome-id';
    const chromeAppTitle = 'Chrome';
    observer.onAppInstalledOrUpdated({
      id: chromeAppId,
      title: chromeAppTitle,
      isBlocked: false,
    });
    await flushTasks();

    let appsList = getApps();
    assertEquals(2, appsList.length);
    assertTrue(!!appsList[0]);
    assertTrue(!!appsList[1]);

    let title1 =
        appsList[0].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title1);
    assertEquals(title1.innerText, chromeAppTitle);

    const title2 =
        appsList[1].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title2);
    assertEquals(title2.innerText, filesAppTitle);

    observer.onAppUninstalled({
      id: chromeAppId,
      title: chromeAppTitle,
      isBlocked: false,
    });
    await flushTasks();

    appsList = getApps();
    assertEquals(1, getApps().length);
    assertTrue(!!appsList[0]);

    title1 = appsList[0].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title1);
    assertEquals(title1.innerText, filesAppTitle);
  });

  test('Clicking app row updates app blocked state', async () => {
    const app = createApp('file-id', 'Files', false);
    handler.addAppForTesting(app);

    await createPage();

    const apps = getApps();
    assertEquals(1, apps.length);

    const appElement = apps[0];
    assertTrue(!!appElement);
    appElement.click();

    assertEquals(handler.getCallCount('updateApp'), 1);
    const args1 = handler.getArgs('updateApp')[0];
    assertEquals(2, args1.length);
    assertEquals(app.id, args1[0]);
    assertTrue(/*isBlocked*/ args1[1]);
    assertTrue(app.isBlocked);

    appElement.click();

    assertEquals(handler.getCallCount('updateApp'), 2);
    const args2 = handler.getArgs('updateApp')[1];
    assertEquals(2, args2.length);
    assertEquals(app.id, args2[0]);
    assertFalse(/*isBlocked*/ args2[1]);
    assertFalse(app.isBlocked);
  });

  test('Remote app changes update toggle state', async () => {
    const app = createApp('file-id', 'Files', false);
    handler.addAppForTesting(app);

    await createPage();

    const apps = getApps();
    assertEquals(1, apps.length);

    const appElement = apps[0];
    assertTrue(!!appElement);
    const appToggle =
        appElement.shadowRoot!.querySelector<CrToggleElement>('.app-toggle');
    assertTrue(!!appToggle);
    assertTrue(appToggle.checked);

    const observer = handler.getObserverRemote();
    assertTrue(!!observer);

    // Dispatch readiness changed event with block state changed.
    observer.onAppInstalledOrUpdated({
      id: app.id,
      title: app.title,
      isBlocked: !app.isBlocked,
    });
    await flushTasks();

    assertFalse(appToggle.checked);
  });

  test(
      'Readiness events without block state changes do not update list',
      async () => {
        const app = createApp('file-id', 'Files', false);
        handler.addAppForTesting(app);

        await createPage();

        const observer = handler.getObserverRemote();
        assertTrue(!!observer);

        const apps = getApps();
        assertEquals(1, apps.length);

        const appElement = apps[0];
        assertTrue(!!appElement);
        const appToggle = appElement.shadowRoot!.querySelector<CrToggleElement>(
            '.app-toggle');
        assertTrue(!!appToggle);
        assertTrue(appToggle.checked);

        // Dispatch readiness changed event without any changes to block state.
        observer.onAppInstalledOrUpdated(app);

        assertTrue(appToggle.checked);
        assertFalse(app.isBlocked);

        await flushTasks();

        const blockedAppsCount =
            page.shadowRoot!.querySelector<HTMLElement>('#blockedAppsCount');
        assertTrue(!!blockedAppsCount);
        assertEquals(blockedAppsCount.innerText, '0 of 1 apps blocked');
      });

  test('Unverified page redirects to Apps', async () => {
    Router.getInstance().navigateTo(routes.APP_PARENTAL_CONTROLS);
    // Page is unverified by default and will redirect.
    page = new SettingsAppParentalControlsSubpageElement();
    document.body.appendChild(page);
    await flushTasks();

    assertEquals(Router.getInstance().currentRoute, routes.APPS);
  });

  test('Blocked apps string is not shown when there are no apps', async () => {
    await createPage();
    await flushTasks();

    const blockedAppsCount =
        page.shadowRoot!.querySelector<HTMLElement>('#blockedAppsCount');
    assertFalse(isVisible(blockedAppsCount));
  });
});

suite('AppParentalControlsSubpage search', () => {
  let page: SettingsAppParentalControlsSubpageElement;
  let handler: FakeAppParentalControlsHandler;

  async function createPage(): Promise<void> {
    Router.getInstance().navigateTo(routes.APP_PARENTAL_CONTROLS);
    page = new SettingsAppParentalControlsSubpageElement();
    // Set verified to true to indicate pin was accepted on the apps page.
    page.set('isVerified', true);
    document.body.appendChild(page);
    await flushTasks();
  }

  function getApps(): NodeListOf<HTMLElement> {
    const appList = page.shadowRoot!.querySelector('#appParentalControlsList');
    assertTrue(!!appList);
    assertTrue(isVisible(appList));
    return appList.querySelectorAll<HTMLElement>('block-app-item');
  }

  setup(() => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    page.remove();
  });

  test('Search returns one result', async () => {
    const filesAppTitle = 'Files';
    handler.addAppForTesting(createApp('file-id', filesAppTitle, false));
    handler.addAppForTesting(createApp('chrome-id', 'Chrome', false));

    await createPage();

    assertEquals(2, getApps().length);

    page.searchTerm = 'f';
    await flushTasks();

    assertEquals(1, getApps().length);
    const app = getApps()[0];
    assertTrue(!!app);
    const appTitle = app.shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!appTitle);
    assertEquals(appTitle.innerText, filesAppTitle);
  });

  test('Search returns multiple results', async () => {
    const cameraAppTitle = 'Camera';
    const chromeAppTitle = 'Chrome';
    handler.addAppForTesting(createApp('file-id', 'Files', false));
    handler.addAppForTesting(createApp('chrome-id', chromeAppTitle, false));
    handler.addAppForTesting(createApp('camera-id', cameraAppTitle, false));

    await createPage();

    assertEquals(3, getApps().length);

    page.searchTerm = 'c';
    await flushTasks();

    assertEquals(2, getApps().length);

    const app1 = getApps()[0];
    assertTrue(!!app1);
    const appTitle1 = app1.shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!appTitle1);
    assertEquals(appTitle1.innerText, cameraAppTitle);

    const app2 = getApps()[1];
    assertTrue(!!app2);
    const appTitle2 = app2.shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!appTitle2);
    assertEquals(appTitle2.innerText, chromeAppTitle);
  });

  test('Search returns no results', async () => {
    const filesAppTitle = 'Files';
    handler.addAppForTesting(createApp('file-id', filesAppTitle, false));
    handler.addAppForTesting(createApp('chrome-id', 'Chrome', false));

    await createPage();

    assertEquals(2, getApps().length);

    page.searchTerm = 'zzz';
    await flushTasks();

    assertEquals(0, getApps().length);
    const emptyAppList =
        page.shadowRoot!.querySelector<HTMLElement>('#noAppsLabel');
    assertTrue(!!emptyAppList);
    assertTrue(isVisible(emptyAppList));
  });


  test('Blocked apps string is not shown when search string is not empty', async () => {
    handler.addAppForTesting(createApp('file-id', 'Files', false));
    handler.addAppForTesting(createApp('chrome-id', 'Chrome', false));

    await createPage();

    assertEquals(2, getApps().length);

    page.searchTerm = 'f';
    await flushTasks();

    const blockedAppsCount =
    page.shadowRoot!.querySelector<HTMLElement>('#blockedAppsCount');
    assertFalse(isVisible(blockedAppsCount));
  });
});
