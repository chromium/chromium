// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
import {flushTasks} from 'chrome://test/test_util.js';

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
      type: apps.mojom.AppType.kWeb,
      installSource: apps.mojom.InstallSource.kUnknown,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'Web App');
  });

  test('Android type', async function() {
    const options = {
      type: apps.mojom.AppType.kArc,
      installSource: apps.mojom.InstallSource.kUnknown,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'Android App');
  });

  test('Chrome type', async function() {
    const options = {
      type: apps.mojom.AppType.kChromeApp,
      installSource: apps.mojom.InstallSource.kUnknown,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'Chrome App');
  });

  test('Chrome App from web store', async function() {
    const options = {
      type: apps.mojom.AppType.kChromeApp,
      installSource: apps.mojom.InstallSource.kChromeWebStore,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'Chrome App installed from <a href="#">Chrome Web Store</a>');
  });

  test('Android App from play store', async function() {
    const options = {
      type: apps.mojom.AppType.kArc,
      installSource: apps.mojom.InstallSource.kPlayStore,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'Android App installed from <a href="#">Google Play Store</a>');
  });

  test('System type', async function() {
    const options = {
      type: apps.mojom.AppType.kSystemWeb,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'System App');
  });

  test('system install source', async function() {
    const options = {
      installSource: apps.mojom.InstallSource.kSystem,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#type-and-source'));
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type-and-source')
            .textContent.trim(),
        'ChromeOS System App');
  });

  test('Chrome app version', async function() {
    const options = {
      type: apps.mojom.AppType.kChromeApp,
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

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#version').textContent.trim(),
        'Version: 17.2');
  });

  test('Android app version', async function() {
    const options = {
      type: apps.mojom.AppType.kArc,
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

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#version').innerText.trim(),
        'Version: 13.1.52');
  });

  test('Android type storage', async function() {
    const options = {
      type: apps.mojom.AppType.kArc,
      installSource: apps.mojom.InstallSource.kUnknown,
      appSize: '17 MB',
    };

    const options2 = {
      type: apps.mojom.AppType.kArc,
      installSource: apps.mojom.InstallSource.kUnknown,
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

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#storage-title'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#app-size'));
    assertFalse(!!appDetailsItem.shadowRoot.querySelector('#data-size'));

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#app-size').textContent.trim(),
        'App size: 17 MB');

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app2.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app2.id]);

    appDetailsItem.app = app2;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#storage-title'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#app-size'));
    assertTrue(!!appDetailsItem.shadowRoot.querySelector('#data-size'));

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#app-size').textContent.trim(),
        'App size: 17 MB');
    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#data-size')
            .textContent.trim(),
        'Data stored in app: 124.6 GB');
  });
});
