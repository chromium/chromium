// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType, InstallSource} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {replaceBody, replaceStore, setupFakeHandler} from './test_util.js';

suite('<app-management-app-details-item>', () => {
  let appDetailsItem;
  let fakeHandler;

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    appDetailsItem = document.createElement('app-management-app-details-item');

    replaceBody(appDetailsItem);
    flushTasks();
  });

  /**
   * @param {Object} appOptions
   * @param {string} [appName="app"]
   **/
  async function addApp(appOptions, appName = 'app') {
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

  test('PWA type from unknown source', async function() {
    await addApp({
      type: AppType.kWeb,
      installSource: InstallSource.kUnknown,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Web App');
  });

  test('PWA type from browser', async function() {
    const publisherId = 'https://google.com/';
    await addApp({
      type: AppType.kWeb,
      installSource: InstallSource.kBrowser,
      publisherId,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSourceText'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSourceText')
            .textContent.trim(),
        'Web App installed from Chrome browser');

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#infoIconTooltip')
                     .querySelector('#tooltipText'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#infoIconTooltip')
            .querySelector('#tooltipText')
            .textContent.trim(),
        publisherId);
  });

  test('Android type', async function() {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Android App');
  });

  test('Chrome type', async function() {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kUnknown,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Chrome App');
  });

  test('Chrome App from web store', async function() {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kChromeWebStore,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Chrome App installed from <a href="#">Chrome Web Store</a>');
  });

  test('Android App from play store', async function() {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kPlayStore,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Android App installed from <a href="#">Google Play Store</a>');
  });

  test('System type', async function() {
    await addApp({
      type: AppType.kSystemWeb,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'System App');
  });

  test('System install source', async function() {
    await addApp({
      installSource: InstallSource.kSystem,
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'ChromeOS System App');
  });

  test('Chrome app version', async function() {
    await addApp({
      type: AppType.kChromeApp,
      version: '17.2',
    });

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#version').textContent.trim(),
        'Version: 17.2');
  });

  test('Android app version', async function() {
    await addApp({
      type: AppType.kArc,
      version: '13.1.52',
    });

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#version').innerText.trim(),
        'Version: 13.1.52');
  });

  test('Android type storage', async function() {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
      appSize: '17 MB',
    });

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#storageTitle'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#appSize'));
    assertFalse(!!appDetailsItem.shadowRoot.querySelector('#dataSize'));

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#appSize').textContent.trim(),
        'App size: 17 MB');

    await addApp(
        {
          type: AppType.kArc,
          installSource: InstallSource.kUnknown,
          appSize: '17 MB',
          dataSize: '124.6 GB',
        },
        'app2');

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#storageTitle'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#appSize'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#dataSize'));

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#appSize').textContent.trim(),
        'App size: 17 MB');
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#dataSize').textContent.trim(),
        'Data stored in app: 124.6 GB');
  });
});
