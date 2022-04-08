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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
        'Chrome App installed from Chrome Web Store');
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
        'Android App installed from Google Play Store');
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
        'Chrome OS System App');
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#version').innerText.trim(),
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

    assertEquals(
        appDetailsItem.shadowRoot.querySelector('#version').innerText.trim(),
        'Version: 13.1.52');
  });
});
