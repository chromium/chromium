// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-supported-links-item>', () => {
  let supportedLinksItem;
  let fakeHandler;

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    supportedLinksItem =
        document.createElement('app-management-supported-links-item');
    // TODO(ajlinker): Remove this line when the feature flag is removed.
    supportedLinksItem.appManagementIntentSettingsEnabled_ = true;
  });

  test(
      'PWA - preferred -> browser',
      async function() {
        const pwaOptions = {
          type: apps.mojom.AppType.kWeb,
          isPreferredApp: true,
          supportedLinks: ['google.com'],
        };

        // Add PWA app, and make it the currently selected app.
        const app = await fakeHandler.addApp('app1', pwaOptions);

        app_management.AppManagementStore.getInstance().dispatch(
            app_management.actions.updateSelectedAppId(app.id));

        await fakeHandler.flushPipesForTesting();

        assertTrue(!!app_management.AppManagementStore.getInstance()
                         .data.apps[app.id]);

        supportedLinksItem.app = app;

        replaceBody(supportedLinksItem);
        fakeHandler.flushPipesForTesting();
        test_util.flushTasks();

        expectEquals(
            supportedLinksItem.$$('cr-radio-group').selected, 'preferred');

        await supportedLinksItem.$$('#browser').click();
        await fakeHandler.flushPipesForTesting();
        await test_util.flushTasks();

        expectFalse(app_management.AppManagementStore.getInstance()
                        .data.apps[app.id]
                        .isPreferredApp);

        expectEquals(
            supportedLinksItem.$$('cr-radio-group').selected, 'browser');
      }),

      test('ARC - browser -> preferred', async function() {
        const arcOptions = {
          type: apps.mojom.AppType.kArc,
          isPreferredApp: false,
          supportedLinks: ['google.com', 'gmail.com'],
        };

        // Add ARC app, and make it the currently selected app.
        const app = await fakeHandler.addApp('app1', arcOptions);

        app_management.AppManagementStore.getInstance().dispatch(
            app_management.actions.updateSelectedAppId(app.id));

        await fakeHandler.flushPipesForTesting();

        assertTrue(!!app_management.AppManagementStore.getInstance()
                         .data.apps[app.id]);

        supportedLinksItem.app = app;

        replaceBody(supportedLinksItem);
        fakeHandler.flushPipesForTesting();
        test_util.flushTasks();

        expectEquals(
            supportedLinksItem.$$('cr-radio-group').selected, 'browser');

        await supportedLinksItem.$$('#preferred').click();
        await fakeHandler.flushPipesForTesting();
        await test_util.flushTasks();

        expectTrue(app_management.AppManagementStore.getInstance()
                       .data.apps[app.id]
                       .isPreferredApp);

        expectEquals(
            supportedLinksItem.$$('cr-radio-group').selected, 'preferred');
      });

  test('No supported links', async function() {
    const pwaOptions = {
      type: apps.mojom.AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: [],  // Explicitly empty.
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    assertFalse(!!supportedLinksItem.$$('permission-section-header'));
    assertFalse(!!supportedLinksItem.$$('list-frame'));
  });

  test('Window/tab mode', async function() {
    const options = {
      type: apps.mojom.AppType.kWeb,
      isPreferredApp: true,
      windowMode: apps.mojom.WindowMode.kBrowser,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', options);

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    assertTrue(!!supportedLinksItem.$$('#tabModeText'));
    assertTrue(!!supportedLinksItem.$$('#isSupportedRadioGroup').disabled);
  });
});
