// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody, isHidden} from './test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {AppType, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-supported-links-item>', () => {
  let supportedLinksItem;
  let fakeHandler;

  setup(() => {
    PolymerTest.clearBody();
    fakeHandler = setupFakeHandler();
    replaceStore();

    supportedLinksItem =
        document.createElement('app-management-supported-links-item');
  });

  test('PWA - preferred -> browser', async function() {
    const pwaOptions = {
      type: AppType.kWeb,
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
    await fakeHandler.flushPipesForTesting();
    flushTasks();

    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');

    await supportedLinksItem.shadowRoot.querySelector('#browserRadioButton')
        .click();
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
      type: AppType.kArc,
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
    await fakeHandler.flushPipesForTesting();
    flushTasks();

    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');

    await supportedLinksItem.shadowRoot.querySelector('#preferredRadioButton')
        .click();
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
      type: AppType.kWeb,
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
    await fakeHandler.flushPipesForTesting();
    flushTasks();

    assertTrue(isHidden(supportedLinksItem));
  });

  test('Window/tab mode', async function() {
    const options = {
      type: AppType.kWeb,
      isPreferredApp: true,
      windowMode: WindowMode.kBrowser,
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
        '#disabledExplanationText'));
    assertTrue(
        !!supportedLinksItem.shadowRoot.querySelector('#radioGroup').disabled);
  });

  test('can open and close supported link list dialog', async function() {
    const supportedLink = 'google.com';
    const pwaOptions = {
      type: AppType.kWeb,
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

    let supportedLinksDialog =
        supportedLinksItem.shadowRoot.querySelector('#dialog');
    assertEquals(null, supportedLinksDialog);

    // Open dialog.
    const heading = supportedLinksItem.shadowRoot.querySelector('#heading');
    heading.shadowRoot.querySelector('a').click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    supportedLinksDialog =
        supportedLinksItem.shadowRoot.querySelector('#dialog');
    assertTrue(!!supportedLinksDialog);
    const innerDialog =
        supportedLinksDialog.shadowRoot.querySelector('#dialog');
    assertTrue(innerDialog.open);

    // Confirm google.com shows up.
    assertEquals(
        supportedLinksDialog.shadowRoot.querySelector('#list')
            .getElementsByClassName('list-item')[0]
            .innerText,
        supportedLink);

    // Close dialog.
    innerDialog.shadowRoot.querySelector('#close').click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // Wait for the stamped dialog to be destroyed.
    await waitAfterNextRender(supportedLinksDialog);
    supportedLinksDialog =
        supportedLinksItem.shadowRoot.querySelector('#dialog');
    assertEquals(null, supportedLinksDialog);
  });

  // TODO(crbug/1253891): Race condition when closing the dialog makes this test
  // flaky.
  test.skip('overlap dialog is shown and cancelled', async function() {
    const pwaOptions = {
      type: AppType.kWeb,
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
    let overlapDialog = supportedLinksItem.querySelector('#overlapDialog');
    assertEquals(null, overlapDialog);
    assertTrue(
        supportedLinksItem.shadowRoot.querySelector('#browserRadioButton')
            .checked);

    // Open dialog
    const promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    await supportedLinksItem.shadowRoot.querySelector('#preferredRadioButton')
        .click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    overlapDialog =
        supportedLinksItem.shadowRoot.querySelector('#overlapDialog');
    assertTrue(!!overlapDialog);

    // Close dialog
    overlapDialog.shadowRoot.querySelector('#cancel').click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    overlapDialog =
        supportedLinksItem.shadowRoot.querySelector('#overlapDialog');
    assertEquals(null, overlapDialog);

    assertFalse(
        AppManagementStore.getInstance().data.apps[app.id].isPreferredApp);
    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'browser');
  });

  test('overlap dialog is shown and accepted', async function() {
    const pwaOptions = {
      type: AppType.kWeb,
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
    assertFalse(!!supportedLinksItem.querySelector('#overlapDialog'));
    assertTrue(
        supportedLinksItem.shadowRoot.querySelector('#browserRadioButton')
            .checked);

    // Open dialog
    let promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    await supportedLinksItem.shadowRoot.querySelector('#preferredRadioButton')
        .click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();
    assertTrue(!!supportedLinksItem.shadowRoot.querySelector('#overlapDialog'));

    // Accept change
    promise = fakeHandler.whenCalled('setPreferredApp');
    supportedLinksItem.shadowRoot.querySelector('#overlapDialog')
        .$.change.click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertFalse(
        !!supportedLinksItem.shadowRoot.querySelector('#overlapDialog'));
    assertTrue(
        AppManagementStore.getInstance().data.apps[app.id].isPreferredApp);
    assertEquals(
        supportedLinksItem.shadowRoot.querySelector('cr-radio-group').selected,
        'preferred');
  });

  test('overlap warning isnt shown when not selected', async function() {
    const pwaOptions1 = {
      type: AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    const pwaOptions2 = {
      type: AppType.kWeb,
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
        !!supportedLinksItem.shadowRoot.querySelector('#overlapWarning'));
  });

  test('overlap warning is shown', async function() {
    const pwaOptions1 = {
      type: AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    const pwaOptions2 = {
      type: AppType.kWeb,
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
        !!supportedLinksItem.shadowRoot.querySelector('#overlapWarning'));
  });
});
