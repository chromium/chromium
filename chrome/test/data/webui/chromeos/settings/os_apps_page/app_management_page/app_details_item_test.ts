// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementAppDetailsItem} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {App, AppType, InstallReason, InstallSource} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-app-details-item>', () => {
  let appDetailsItem: AppManagementAppDetailsItem;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    loadTimeData.overrideValues({appManagementDeviceName: 'Chromebook'});

    appDetailsItem = document.createElement('app-management-app-details-item');

    replaceBody(appDetailsItem);
    flushTasks();
  });

  teardown(() => {
    appDetailsItem.remove();
    loadTimeData.overrideValues({appManagementDeviceName: 'Chrome device'});
  });

  async function addApp(appOptions: Partial<App>, appName: string = 'app') {
    // Add an app, and make it the currently selected app.
    const app = await fakeHandler.addApp(appName, appOptions);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();
  }

  test('PWA type from unknown source', async () => {
    await addApp({
      type: AppType.kWeb,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('Web App', typeAndSource.textContent!.trim());
  });

  test('PWA type from browser', async () => {
    const publisherId = 'https://google.com/';
    await addApp({
      type: AppType.kWeb,
      installSource: InstallSource.kBrowser,
      publisherId,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Web App installed from Chrome browser',
        typeAndSourceText.textContent!.trim());

    const infoIconTooltip =
        appDetailsItem.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!infoIconTooltip);
    assertEquals(publisherId, infoIconTooltip.tooltipText!.trim());
  });

  test('Android type', async () => {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('Android App', typeAndSource.textContent!.trim());
  });

  test('Chrome type', async () => {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('Chrome App', typeAndSource.textContent!.trim());
  });

  test('Unknown type', async () => {
    await addApp({
      type: AppType.kUnknown,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('', typeAndSource.textContent!.trim());
  });

  test('Chrome App from web store', async () => {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kChromeWebStore,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals(
        'Chrome App installed from <a href="#">Chrome Web Store</a>',
        typeAndSource.textContent!.trim());

    const launchIcon = appDetailsItem.shadowRoot!.querySelector('#launchIcon');
    assertTrue(!!launchIcon);
  });

  test('Chrome type storage', async () => {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kUnknown,
      appSize: '17 MB',
    });

    const appSize = appDetailsItem.shadowRoot!.querySelector('#appSize');

    assertTrue(!!appDetailsItem.shadowRoot!.querySelector('#storageTitle'));
    assertTrue(!!appSize);
    assertNull(appDetailsItem.shadowRoot!.querySelector('#dataSize'));

    assertEquals('App size: 17 MB', appSize.textContent!.trim());
  });

  test('Android App from play store', async () => {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kPlayStore,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals(
        'Android App installed from <a href="#">Google Play Store</a>',
        typeAndSource.textContent!.trim());

    const launchIcon = appDetailsItem.shadowRoot!.querySelector('#launchIcon');
    assertTrue(!!launchIcon);
  });

  test('System install source', async function() {
    await addApp({
      installReason: InstallReason.kSystem,
      installSource: InstallSource.kSystem,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'ChromeOS System App preinstalled on your Chromebook',
        typeAndSourceText.textContent!.trim());
  });

  test('Android App Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kArc,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Android app installed by your device administrator.',
        typeAndSourceText.textContent!.trim());
  });

  test('Chrome App Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kChromeApp,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Chrome app installed by your device administrator.',
        typeAndSourceText.textContent!.trim());
  });

  test('Web App Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kWeb,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Web app installed by your device administrator.',
        typeAndSourceText.textContent!.trim());
  });

  test('No app type Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kUnknown,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals('', typeAndSourceText.textContent!.trim());
  });

  test('Chrome app version', async () => {
    await addApp({
      type: AppType.kChromeApp,
      version: '17.2',
    });

    const version = appDetailsItem.shadowRoot!.querySelector('#version');
    assertTrue(!!version);
    assertEquals('Version: 17.2', version.textContent!.trim());
  });

  test('Android app version', async () => {
    await addApp({
      type: AppType.kArc,
      version: '13.1.52',
    });

    const version =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>('#version');
    assertTrue(!!version);
    assertEquals('Version: 13.1.52', version.innerText!.trim());
  });

  test('Android type storage', async () => {
    await addApp(
        {
          type: AppType.kArc,
          installSource: InstallSource.kUnknown,
          appSize: '17 MB',
          dataSize: '124.6 GB',
        },
        'app2');

    const appSize = appDetailsItem.shadowRoot!.querySelector('#appSize');
    const dataSize = appDetailsItem.shadowRoot!.querySelector('#dataSize');

    assertTrue(!!appDetailsItem.shadowRoot!.querySelector('#storageTitle'));
    assertTrue(!!appSize);
    assertTrue(!!dataSize);

    assertEquals('App size: 17 MB', appSize.textContent!.trim());
    assertEquals('Data stored in app: 124.6 GB', dataSize.textContent!.trim());
  });
});
