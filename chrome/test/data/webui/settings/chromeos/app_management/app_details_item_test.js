// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
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

  test('PWA type', async function() {
    const options = {
      type: AppType.kWeb,
      installSource: InstallSource.kUnknown,
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Web App');
  });

  test('Android type', async function() {
    const options = {
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
    };

    // Add Android app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Android App');
  });

  test('Chrome type', async function() {
    const options = {
      type: AppType.kChromeApp,
      installSource: InstallSource.kUnknown,
    };

    // Add Chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Chrome App');
  });

  test('Chrome App from web store', async function() {
    const options = {
      type: AppType.kChromeApp,
      installSource: InstallSource.kChromeWebStore,
    };

    // Add Chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Chrome App installed from <a href="#">Chrome Web Store</a>');
  });

  test('Android App from play store', async function() {
    const options = {
      type: AppType.kArc,
      installSource: InstallSource.kPlayStore,
    };

    // Add Chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'Android App installed from <a href="#">Google Play Store</a>');
  });

  test('System type', async function() {
    const options = {
      type: AppType.kSystemWeb,
    };

    // Add System app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'System App');
  });

  test('system install source', async function() {
    const options = {
      installSource: InstallSource.kSystem,
    };

    // Add System app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#typeAndSource'));
    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#typeAndSource')
            .textContent.trim(),
        'ChromeOS System App');
  });

  test('Chrome app version', async function() {
    const options = {
      type: AppType.kChromeApp,
      version: '17.2',
    };

    // Add Chrome app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#version').textContent.trim(),
        'Version: 17.2');
  });

  test('Android app version', async function() {
    const options = {
      type: AppType.kArc,
      version: '13.1.52',
    };

    // Add Android app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#version').innerText.trim(),
        'Version: 13.1.52');
  });

  test('Android type storage', async function() {
    const options = {
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
      appSize: '17 MB',
    };

    const options2 = {
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
      appSize: '17 MB',
      dataSize: '124.6 GB',
    };

    // Add Android app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);
    const app2 = await fakeHandler.addApp('app2', options2);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#storageTitle'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#appSize'));
    assertFalse(!!appDetailsItem.shadowRoot.querySelector('#dataSize'));

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#appSize').textContent.trim(),
        'App size: 17 MB');

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app2.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app2.id]);

    appDetailsItem.app = app2;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

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
