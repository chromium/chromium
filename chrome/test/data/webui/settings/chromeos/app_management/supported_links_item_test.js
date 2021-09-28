// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, isHidden} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
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
    // TODO(crbug.com/1204324): Remove this line when the feature is launched.
    loadTimeData.overrideValues({
      appManagementIntentSettingsEnabled: true,
    });
  });

  test('PWA - preferred -> browser', async function() {
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

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');

    await supportedLinksItem.shadowRoot.querySelector('#browser').click();
    await fakeHandler.whenCalled('setPreferredApp');
    await test_util.flushTasks();

    expectFalse(app_management.AppManagementStore.getInstance()
                    .data.apps[app.id]
                    .isPreferredApp);

    expectEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');
  });

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

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    test_util.flushTasks();

    expectEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');

    await supportedLinksItem.shadowRoot.querySelector('#preferred').click();
    await fakeHandler.whenCalled('setPreferredApp');
    await test_util.flushTasks();

    expectTrue(app_management.AppManagementStore.getInstance()
                   .data.apps[app.id]
                   .isPreferredApp);

    expectEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');
  });

  test('No supported links', async function() {
    const pwaOptions = {
      type: apps.mojom.AppType.kWeb,
      isPreferredApp: false,  // Cannot be preferred app if there are no links.
      supportedLinks: [],     // Explicitly empty.
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

    assertTrue(isHidden(supportedLinksItem));
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
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#explanation-text'));
    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#radio-group').disabled);
  });

  test('can open and close supported link list dialog', async function() {
    const supportedLink = 'google.com';
    const pwaOptions = {
      type: apps.mojom.AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: [supportedLink],
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
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    assertFalse(!!supportedLinksItem.querySelector('#dialog'));

    // Open dialog.
    const heading = supportedLinksItem.shadowRoot.querySelector('#heading');
    heading.shadowRoot.querySelector('a').click();
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();
    const dialog = supportedLinksItem.shadowRoot.querySelector('#dialog')
                       .shadowRoot.querySelector('#dialog');
    assertTrue(dialog.open);

    // Confirm google.com shows up.
    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('#dialog')
            .shadowRoot.querySelector('#list')
            .getElementsByClassName('list-item')[0]
            .innerText,
        supportedLink);

    // Close dialog.
    dialog.shadowRoot.querySelector('#close').click();
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();
    assertFalse(supportedLinksItem.shadowRoot.querySelector('#dialog')
                    .shadowRoot.querySelector('#dialog')
                    .open);
  });

  test('overlap dialog is shown and cancelled', async function() {
    const pwaOptions = {
      type: apps.mojom.AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);
    await fakeHandler.addApp('app2', pwaOptions);
    fakeHandler.overlappingAppIds = ['app2'];

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);
    supportedLinksItem.app = app;
    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    // Pre-test checks
    assertFalse(!!supportedLinksItem.querySelector('#overlap-dialog'));
    assertTrue(supportedLinksItem.$.browser.checked);

    // Open dialog
    const promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    await supportedLinksItem.shadowRoot.querySelector('#preferred').click();
    await promise;
    await test_util.flushTasks();
    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));

    // Close dialog
    supportedLinksItem.shadowRoot.querySelector('#overlap-dialog')
        .$.cancel.click();
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    assertFalse(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));
    expectFalse(app_management.AppManagementStore.getInstance()
                    .data.apps[app.id]
                    .isPreferredApp);
    expectEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');
  });

  test('overlap dialog is shown and accepted', async function() {
    const pwaOptions = {
      type: apps.mojom.AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);
    await fakeHandler.addApp('app2', pwaOptions);
    fakeHandler.overlappingAppIds = ['app2'];

    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);
    supportedLinksItem.app = app;
    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    // Pre-test checks
    assertFalse(!!supportedLinksItem.querySelector('#overlap-dialog'));
    assertTrue(supportedLinksItem.$.browser.checked);

    // Open dialog
    let promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    await supportedLinksItem.shadowRoot.querySelector('#preferred').click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();
    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));

    // Accept change
    promise = fakeHandler.whenCalled('setPreferredApp');
    supportedLinksItem.shadowRoot.querySelector('#overlap-dialog')
        .$.change.click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await test_util.flushTasks();

    assertFalse(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));
    expectTrue(app_management.AppManagementStore.getInstance()
                   .data.apps[app.id]
                   .isPreferredApp);
    expectEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');
  });
});
