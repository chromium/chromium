// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementSupportedLinksItemElement, AppManagementSupportedLinksOverlappingAppsDialogElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {AppType, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {isHidden, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-supported-links-item>', () => {
  let supportedLinksItem: AppManagementSupportedLinksItemElement;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    supportedLinksItem =
        document.createElement('app-management-supported-links-item');
  });

  teardown(() => {
    supportedLinksItem.remove();
  });

  test('PWA - preferred -> browser', async () => {
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

    let radioGroup =
        supportedLinksItem.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('preferred', radioGroup.selected);

    const browserRadioButton =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#browserRadioButton');
    assertTrue(!!browserRadioButton);
    await browserRadioButton.click();
    await fakeHandler.whenCalled('setPreferredApp');
    await flushTasks();

    const selectedApp = AppManagementStore.getInstance().data.apps[app.id];
    assertTrue(!!selectedApp);
    assertFalse(selectedApp.isPreferredApp);

    radioGroup = supportedLinksItem.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('browser', radioGroup.selected);
  });

  test('ARC - browser -> preferred', async () => {
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

    let radioGroup =
        supportedLinksItem.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('browser', radioGroup.selected);

    const preferredRadioButton =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#preferredRadioButton');
    assertTrue(!!preferredRadioButton);
    await preferredRadioButton.click();
    await fakeHandler.whenCalled('setPreferredApp');
    await flushTasks();

    const selectedApp = AppManagementStore.getInstance().data.apps[app.id];
    assertTrue(!!selectedApp);
    assertTrue(selectedApp.isPreferredApp);

    radioGroup = supportedLinksItem.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('preferred', radioGroup.selected);
  });

  test('No supported links', async () => {
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

  test('Window/tab mode', async () => {
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

    assertTrue(!!supportedLinksItem.shadowRoot!.querySelector(
        '#disabledExplanationText'));

    const radioGroup =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioGroupElement>(
            '#radioGroup');
    assertTrue(!!radioGroup);
    assertTrue(!!radioGroup.disabled);
  });

  test('can open and close supported link list dialog', async () => {
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
        supportedLinksItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertNull(supportedLinksDialog);

    // Open dialog.
    const heading = supportedLinksItem.shadowRoot!.querySelector('#heading');
    assertTrue(!!heading);
    const link = heading.shadowRoot!.querySelector('a');
    assertTrue(!!link);
    link.click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    supportedLinksDialog =
        supportedLinksItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertTrue(!!supportedLinksDialog);
    const innerDialog =
        supportedLinksDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!innerDialog);
    assertTrue(innerDialog.open);

    // Confirm google.com shows up.
    const list = supportedLinksDialog.shadowRoot!.querySelector('#list');
    assertTrue(!!list);
    const item = list.getElementsByClassName('list-item')[0] as HTMLElement;
    assertTrue(!!item);
    assertEquals(supportedLink, item.innerText);

    // Close dialog.
    const closeButton =
        innerDialog.shadowRoot!.querySelector<HTMLButtonElement>('#close');
    assertTrue(!!closeButton);
    closeButton.click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // Wait for the stamped dialog to be destroyed.
    await waitAfterNextRender(supportedLinksDialog);
    supportedLinksDialog =
        supportedLinksItem.shadowRoot!.querySelector('#dialog');
    assertNull(supportedLinksDialog);
  });

  // TODO(crbug/1253891): Race condition when closing the dialog makes this test
  // flaky.
  test.skip('overlap dialog is shown and cancelled', async () => {
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
    assertNull(overlapDialog);

    const browserRadioButton =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#browserRadioButton');
    assertTrue(!!browserRadioButton);
    assertTrue(browserRadioButton.checked);

    // Open dialog
    const promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    const preferredRadioButton =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#preferredRadioButton');
    assertTrue(!!preferredRadioButton);
    await preferredRadioButton.click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    overlapDialog =
        supportedLinksItem.shadowRoot!.querySelector('#overlapDialog');
    assertTrue(!!overlapDialog);

    // Close dialog
    const cancelButton =
        overlapDialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    overlapDialog =
        supportedLinksItem.shadowRoot!.querySelector('#overlapDialog');
    assertNull(overlapDialog);

    const selectedApp = AppManagementStore.getInstance().data.apps[app.id];
    assertTrue(!!selectedApp);
    assertFalse(selectedApp.isPreferredApp);
    const radioGroup =
        supportedLinksItem.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('browser', radioGroup.selected);
  });

  test('overlap dialog is shown and accepted', async () => {
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
    assertNull(supportedLinksItem.querySelector('#overlapDialog'));
    const browserRadioButton =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#browserRadioButton');
    assertTrue(!!browserRadioButton);
    assertTrue(browserRadioButton.checked);

    // Open dialog
    let promise = fakeHandler.whenCalled('getOverlappingPreferredApps');
    const preferredRadioButton =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#preferredRadioButton');
    assertTrue(!!preferredRadioButton);
    await preferredRadioButton.click();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();
    assertTrue(
        !!supportedLinksItem.shadowRoot!.querySelector('#overlapDialog'));

    // Accept change
    promise = fakeHandler.whenCalled('setPreferredApp');
    const overlapDialog = supportedLinksItem.shadowRoot!.querySelector<
        AppManagementSupportedLinksOverlappingAppsDialogElement>(
        '#overlapDialog');
    assertTrue(!!overlapDialog);
    overlapDialog.$.dialog.close();
    await promise;
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    assertNull(supportedLinksItem.shadowRoot!.querySelector('#overlapDialog'));

    const selectedApp = AppManagementStore.getInstance().data.apps[app.id];
    assertTrue(!!selectedApp);
    assertTrue(selectedApp.isPreferredApp);
    const radioGroup =
        supportedLinksItem.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('preferred', radioGroup.selected);
  });

  test('overlap warning isnt shown when not selected', async () => {
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

    assertNull(supportedLinksItem.shadowRoot!.querySelector('#overlapWarning'));
  });

  test('overlap warning is shown', async () => {
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
        !!supportedLinksItem.shadowRoot!.querySelector('#overlapWarning'));
  });
});
