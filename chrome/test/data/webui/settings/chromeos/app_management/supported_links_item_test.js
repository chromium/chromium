// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody, isHidden} from './test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<app-management-supported-links-item>', () => {
  let supportedLinksItem;
  let fakeHandler;

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    supportedLinksItem =
        document.createElement('app-management-supported-links-item');

    replaceBody(supportedLinksItem);
    flushTasks();
  });

  test('PWA - preferred -> browser', async function() {
    const pwaOptions = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');

    await supportedLinksItem.shadowRoot.querySelector('#browser').click();
    await fakeHandler.whenCalled('setPreferredApp');
    await flushTasks();

    assertFalse(
        AppManagementStore.getInstance().data.apps[app.id].isPreferredApp);

    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');
  });

  test('ARC - browser -> preferred', async function() {
    const arcOptions = {
      type: appManagement.mojom.AppType.kArc,
      isPreferredApp: false,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    // Add ARC app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', arcOptions);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');

    await supportedLinksItem.shadowRoot.querySelector('#preferred').click();
    await fakeHandler.whenCalled('setPreferredApp');
    await flushTasks();

    assertTrue(
        AppManagementStore.getInstance().data.apps[app.id].isPreferredApp);

    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');
  });

  test('No supported links', async function() {
    const pwaOptions = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: false,  // Cannot be preferred app if there are no links.
      supportedLinks: [],     // Explicitly empty.
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(isHidden(supportedLinksItem));
  });

  test('Window/tab mode', async function() {
    const options = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: true,
      windowMode: appManagement.mojom.WindowMode.kBrowser,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', options);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertTrue(!!supportedLinksItem.shadowRoot.querySelector(
        '#disabled-explanation-text'));
    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#radio-group').disabled);
  });

  // TODO(crbug/1253891): Race condition when closing the dialog makes this test
  // flaky.
  test.skip('can open and close supported link list dialog', async function() {
    const supportedLink = 'google.com';
    const pwaOptions = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: [supportedLink],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    supportedLinksItem.app = app;

    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertFalse(!!supportedLinksItem.querySelector('#dialog'));

    // Open dialog.
    const heading = supportedLinksItem.shadowRoot.querySelector('#heading');
    heading.shadowRoot.querySelector('a').click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();
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
    await flushTasks();
    assertFalse(supportedLinksItem.shadowRoot.querySelector('#dialog')
                    .shadowRoot.querySelector('#dialog')
                    .open);
  });

  // TODO(crbug/1253891): Race condition when closing the dialog makes this test
  // flaky.
  test.skip('overlap dialog is shown and cancelled', async function() {
    const pwaOptions = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);
    await fakeHandler.addApp('app2', pwaOptions);
    fakeHandler.overlappingAppIds = ['app2'];

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);
    supportedLinksItem.app = app;
    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // Pre-test checks
    assertFalse(!!supportedLinksItem.querySelector('#overlap-dialog'));
    assertTrue(supportedLinksItem.$.browser.checked);

    // Open dialog
    const promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    await supportedLinksItem.shadowRoot.querySelector('#preferred').click();
    await promise;

    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));

    // Close dialog
    supportedLinksItem.shadowRoot.querySelector('#overlap-dialog')
        .$.cancel.click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertFalse(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));
    assertFalse(
        AppManagementStore.getInstance().data.apps[app.id].isPreferredApp);
    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');
  });

  test('overlap dialog is shown and accepted', async function() {
    const pwaOptions = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app = await fakeHandler.addApp('app1', pwaOptions);
    await fakeHandler.addApp('app2', pwaOptions);
    fakeHandler.overlappingAppIds = ['app2'];

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);
    supportedLinksItem.app = app;
    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // Pre-test checks
    assertFalse(!!supportedLinksItem.querySelector('#overlap-dialog'));
    assertTrue(supportedLinksItem.$.browser.checked);

    // Open dialog
    let promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    await supportedLinksItem.shadowRoot.querySelector('#preferred').click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();
    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));

    // Accept change
    promise = fakeHandler.whenCalled('setPreferredApp');
    supportedLinksItem.shadowRoot.querySelector('#overlap-dialog')
        .$.change.click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertFalse(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-dialog'));
    assertTrue(
        AppManagementStore.getInstance().data.apps[app.id].isPreferredApp);
    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');
  });

  test('overlap warning isnt shown when not selected', async function() {
    const pwaOptions1 = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    const pwaOptions2 = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app1 = await fakeHandler.addApp('app1', pwaOptions1);
    await fakeHandler.addApp('app2', pwaOptions2);
    fakeHandler.overlappingAppIds = ['app2'];

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app1.id));
    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app1.id]);
    supportedLinksItem.app = app1;
    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertFalse(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-warning'));
  });

  test('overlap warning is shown', async function() {
    const pwaOptions1 = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    const pwaOptions2 = {
      type: appManagement.mojom.AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    const app1 = await fakeHandler.addApp('app1', pwaOptions1);
    const app2 = await fakeHandler.addApp('app2', pwaOptions2);
    fakeHandler.overlappingAppIds = ['app2'];

    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app1.id));
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app1.id]);
    assertTrue(!!AppManagementStore.getInstance().data.apps[app2.id]);
    supportedLinksItem.app = app1;
    replaceBody(supportedLinksItem);
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#overlap-warning'));
  });
});
