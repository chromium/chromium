// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, updateSelectedAppId, addApp} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, isHidden} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
// clang-format on

suite('<app-management-app-details-item>', () => {
  let appDetailsItem;
  let fakeHandler;

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    appDetailsItem = document.createElement('app-management-app-details-item');

    replaceBody(appDetailsItem);
    test_util.flushTasks();
  });

  test('PWA type', async function() {
    const options = {
      type: apps.mojom.AppType.kWeb,
      installSource: apps.mojom.InstallSource.kUnknown,
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
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

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
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

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
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

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
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

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
        'Android App installed from Google Play Store');
  });

  test('System type', async function() {
    const options = {
      type: apps.mojom.AppType.kSystemWeb,
    };

    // Add System app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
        'System App');
  });

  test('system install source', async function() {
    const options = {
      installSource: apps.mojom.InstallSource.kSystem,
    };

    // Add System app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app', options);

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    appDetailsItem.app = app;

    replaceBody(appDetailsItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
        appDetailsItem.shadowRoot.querySelector('#type').innerText.trim(),
        'Chrome OS System App');
  });
});
