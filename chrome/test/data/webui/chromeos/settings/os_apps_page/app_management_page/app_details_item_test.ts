// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {AppManagementAppDetailsItem} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, InstallReason, InstallSource} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import type {FakePageHandler} from '../../app_management/fake_page_handler.js';
import type {TestAppManagementStore} from '../../app_management/test_store.js';
import {replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-app-details-item>', () => {
  let appDetailsItem: AppManagementAppDetailsItem;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    loadTimeData.overrideValues({appManagementDeviceName: 'Chromebook'});

    appDetailsItem = document.createElement('app-management-app-details-item');

    replaceBody(appDetailsItem);
    flushTasks();
  });

  teardown(() => {
    appDetailsItem.remove();
    loadTimeData.overrideValues({appManagementDeviceName: 'Chrome device'});
  });

  async function addApp(appOptions: Partial<App>, appName: string = 'app') {
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

  test('PWA type from unknown source', async () => {
    await addApp({
      type: AppType.kWeb,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('Web App', typeAndSource.textContent.trim());
  });

  test('PWA type from browser', async () => {
    const publisherId = 'https://google.com/';
    await addApp({
      type: AppType.kWeb,
      installSource: InstallSource.kBrowser,
      publisherId,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Web App installed from Chrome browser',
        typeAndSourceText.textContent.trim());

    const infoIconTooltip =
        appDetailsItem.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!infoIconTooltip);
    assertEquals(publisherId, infoIconTooltip.tooltipText.trim());
  });

  test(
      'IWA type from browser with parent app (data sharing explanation)',
      async () => {
        await fakeHandler.addApp('parent_id', {
          type: AppType.kWeb,
          title: 'Parent App',
        });

        const store =
            AppManagementStore.getInstance() as TestAppManagementStore;
        assertTrue(
            !!store.data.apps['parent_id'], 'Parent app should be in store');

        await addApp(
            {
              type: AppType.kWeb,
              installSource: InstallSource.kBrowser,
            },
            'sub_id');

        assertTrue(!!store.data.apps['sub_id'], 'Sub app should be in store');

        store.data.subAppToParentAppId = {
          ...store.data.subAppToParentAppId,
          'sub_id': 'parent_id',
        };
        store.notifyObservers();
        await flushTasks();

        const subappDataSharingExplanation =
            appDetailsItem.shadowRoot!.querySelector(
                '#subappDataSharingExplanation');
        assertTrue(
            !!subappDataSharingExplanation, 'Explanation should be visible');
        assertEquals(
            'This app data is shared between "Parent App" and its installed apps',
            subappDataSharingExplanation.textContent.trim());
      });

  test('Android type', async () => {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('Android App', typeAndSource.textContent.trim());
  });

  test('Chrome type', async () => {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('Chrome App', typeAndSource.textContent.trim());
  });

  test('Unknown type', async () => {
    await addApp({
      type: AppType.kUnknown,
      installSource: InstallSource.kUnknown,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals('', typeAndSource.textContent.trim());
  });

  test('Chrome App from web store', async () => {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kChromeWebStore,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals(
        'Chrome App installed from <a href="#">Chrome Web Store</a>',
        typeAndSource.textContent.trim());

    const launchIcon = appDetailsItem.shadowRoot!.querySelector('#launchIcon');
    assertTrue(!!launchIcon);
  });

  test('Chrome type storage', async () => {
    await addApp({
      type: AppType.kChromeApp,
      installSource: InstallSource.kUnknown,
      appSize: '17 MB',
    });

    const appSize = appDetailsItem.shadowRoot!.querySelector('#appSize');

    assertTrue(!!appDetailsItem.shadowRoot!.querySelector('#storageTitle'));
    assertTrue(!!appSize);
    assertNull(appDetailsItem.shadowRoot!.querySelector('#dataSize'));

    assertEquals('App size: 17 MB', appSize.textContent.trim());
  });

  test('Android App from play store', async () => {
    await addApp({
      type: AppType.kArc,
      installSource: InstallSource.kPlayStore,
    });

    const typeAndSource =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSource');
    assertTrue(!!typeAndSource);
    assertEquals(
        'Android App installed from <a href="#">Google Play Store</a>',
        typeAndSource.textContent.trim());

    const launchIcon = appDetailsItem.shadowRoot!.querySelector('#launchIcon');
    assertTrue(!!launchIcon);
  });

  test('System install source', async function() {
    await addApp({
      installReason: InstallReason.kSystem,
      installSource: InstallSource.kSystem,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'ChromeOS System App preinstalled on your Chromebook',
        typeAndSourceText.textContent.trim());
  });

  test('Android App Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kArc,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Android app installed by your device administrator.',
        typeAndSourceText.textContent.trim());
  });

  test('Chrome App Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kChromeApp,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Chrome app installed by your device administrator.',
        typeAndSourceText.textContent.trim());
  });

  test('Web App Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kWeb,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals(
        'Web app installed by your device administrator.',
        typeAndSourceText.textContent.trim());
  });

  test('No app type Install reason policy', async function() {
    await addApp({
      installReason: InstallReason.kPolicy,
      type: AppType.kUnknown,
    });

    const typeAndSourceText =
        appDetailsItem.shadowRoot!.querySelector('#typeAndSourceText');
    assertTrue(!!typeAndSourceText);
    assertEquals('', typeAndSourceText.textContent.trim());
  });

  test('Chrome app version', async () => {
    await addApp({
      type: AppType.kChromeApp,
      version: '17.2',
    });

    const version = appDetailsItem.shadowRoot!.querySelector('#version');
    assertTrue(!!version);
    assertEquals('Version: 17.2', version.textContent.trim());
  });

  test('Android app version', async () => {
    await addApp({
      type: AppType.kArc,
      version: '13.1.52',
    });

    const version =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>('#version');
    assertTrue(!!version);
    assertEquals('Version: 13.1.52', version.innerText.trim());
  });

  test('Android type storage', async () => {
    await addApp(
        {
          type: AppType.kArc,
          installSource: InstallSource.kUnknown,
          appSize: '17 MB',
          dataSize: '124.6 GB',
        },
        'app2');

    const appSize = appDetailsItem.shadowRoot!.querySelector('#appSize');
    const dataSize = appDetailsItem.shadowRoot!.querySelector('#dataSize');

    assertTrue(!!appDetailsItem.shadowRoot!.querySelector('#storageTitle'));
    assertTrue(!!appSize);
    assertTrue(!!dataSize);

    assertEquals('App size: 17 MB', appSize.textContent.trim());
    assertEquals('Data stored in app: 124.6 GB', dataSize.textContent.trim());
  });

  test('IWA update section hidden if flag is off', async () => {
    loadTimeData.overrideValues({isIwaInlineUpdateEnabled: false});
    await addApp({
      type: AppType.kWeb,
      publisherId: 'isolated-app://pt2igw6.../',
      version: '1.0.0',
    });

    const updateSection =
        appDetailsItem.shadowRoot!.querySelector('.update-section');
    assertNull(updateSection);
  });

  test('IWA update flow with open windows', async () => {
    loadTimeData.overrideValues({isIwaInlineUpdateEnabled: true});
    await addApp({
      type: AppType.kWeb,
      publisherId: 'isolated-app://pt2igw6.../',
      version: '1.0.0',
      title: 'Kitchen Sink IWA',
    });

    const updateSection =
        appDetailsItem.shadowRoot!.querySelector('.update-section');
    assertTrue(!!updateSection);

    const checkUpdateButton =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
            '#checkUpdateButton');
    assertTrue(!!checkUpdateButton);
    assertEquals('Check for updates', checkUpdateButton.innerText.trim());

    const lastCheckText = appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
        '.last-check-text');
    assertTrue(!!lastCheckText);
    assertEquals('', lastCheckText.innerText.trim());

    fakeHandler.updateVersion = {components: [1, 2, 0]};
    // Mock open windows
    fakeHandler.numWindowsForApp = 1;

    checkUpdateButton.click();
    await fakeHandler.whenCalled('checkForIsolatedWebAppUpdate');
    await fakeHandler.whenCalled('getNumWindowsForApp');
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // Verify Dialog 1 (Update Found Dialog) is shown
    const updateFoundDialog =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
            '#updateFoundDialog');
    assertTrue(!!updateFoundDialog);

    const foundTitle = updateFoundDialog.querySelector('[slot="title"]');
    assertTrue(!!foundTitle);
    assertEquals('Update found', foundTitle.textContent.trim());

    // Should display the merged warning body because windows are open
    const foundBody = updateFoundDialog.querySelector('[slot="body"]');
    assertTrue(!!foundBody);
    assertTrue(foundBody.textContent.includes('1.2.0'));

    // Verify main page button text does NOT change
    assertEquals('Check for updates', checkUpdateButton.innerText.trim());
    assertEquals('', lastCheckText.innerText.trim());

    const confirmFoundButton =
        updateFoundDialog.querySelector<HTMLElement>('#confirmFound');
    assertTrue(!!confirmFoundButton);

    fakeHandler.applyUpdateSuccess = true;
    confirmFoundButton.click();

    await fakeHandler.whenCalled('applyIsolatedWebAppUpdate');
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // All dialogues closed, and button is back to default
    assertNull(appDetailsItem.shadowRoot!.querySelector('#updateFoundDialog'));
    assertEquals('Check for updates', checkUpdateButton.innerText.trim());
    assertEquals('', lastCheckText.innerText.trim());
  });

  test('IWA update flow without open windows', async () => {
    loadTimeData.overrideValues({isIwaInlineUpdateEnabled: true});
    await addApp({
      type: AppType.kWeb,
      publisherId: 'isolated-app://pt2igw6.../',
      version: '1.0.0',
      title: 'Kitchen Sink IWA',
    });

    const checkUpdateButton =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
            '#checkUpdateButton');
    assertTrue(!!checkUpdateButton);

    fakeHandler.updateVersion = {components: [1, 2, 0]};
    // Mock no windows running
    fakeHandler.numWindowsForApp = 0;

    checkUpdateButton.click();
    await fakeHandler.whenCalled('checkForIsolatedWebAppUpdate');
    await fakeHandler.whenCalled('getNumWindowsForApp');
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    const updateFoundDialog =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
            '#updateFoundDialog');
    assertTrue(!!updateFoundDialog);

    const foundTitle = updateFoundDialog.querySelector('[slot="title"]');
    assertTrue(!!foundTitle);
    assertEquals('Update found', foundTitle.textContent.trim());

    // Should display the simple body prompt because windows are not open
    const foundBody = updateFoundDialog.querySelector('[slot="body"]');
    assertTrue(!!foundBody);
    assertTrue(foundBody.textContent.includes('1.2.0'));

    const confirmFoundButton =
        updateFoundDialog.querySelector<HTMLElement>('#confirmFound');
    assertTrue(!!confirmFoundButton);

    fakeHandler.applyUpdateSuccess = true;
    confirmFoundButton.click();

    await fakeHandler.whenCalled('applyIsolatedWebAppUpdate');
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // All dialogues closed
    assertNull(appDetailsItem.shadowRoot!.querySelector('#updateFoundDialog'));
    assertEquals('Check for updates', checkUpdateButton.innerText.trim());
  });

  test('IWA update flow with no update found', async () => {
    loadTimeData.overrideValues({isIwaInlineUpdateEnabled: true});
    await addApp({
      type: AppType.kWeb,
      publisherId: 'isolated-app://pt2igw6.../',
      version: '1.0.0',
      title: 'Kitchen Sink IWA',
    });

    const checkUpdateButton =
        appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
            '#checkUpdateButton');
    assertTrue(!!checkUpdateButton);

    const lastCheckText = appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
        '.last-check-text');
    assertTrue(!!lastCheckText);

    // Mock no update found
    fakeHandler.updateVersion = null;

    checkUpdateButton.click();
    await fakeHandler.whenCalled('checkForIsolatedWebAppUpdate');
    await fakeHandler.flushPipesForTesting();
    await flushTasks();

    // Verify no dialog is shown
    assertNull(appDetailsItem.shadowRoot!.querySelector('#updateFoundDialog'));

    // Verify button is re-enabled, and inline status updates to "App is up to
    // date."
    assertEquals('Check for updates', checkUpdateButton.innerText.trim());
    assertTrue(lastCheckText.innerText.includes('up to date'));
  });

  test(
      'IWA update button disabled and loading state accessibility',
      async () => {
        loadTimeData.overrideValues({isIwaInlineUpdateEnabled: true});
        await addApp({
          type: AppType.kWeb,
          publisherId: 'isolated-app://pt2igw6.../',
          version: '1.0.0',
          title: 'Kitchen Sink IWA',
        });

        const checkUpdateButton =
            appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
                '#checkUpdateButton');
        assertTrue(!!checkUpdateButton);
        // Initially enabled
        assertEquals(null, checkUpdateButton.getAttribute('disabled'));

        fakeHandler.updateVersion = null;

        checkUpdateButton.click();
        await flushTasks();

        // Spinner and disabled state during check
        const spinner =
            appDetailsItem.shadowRoot!.querySelector('paper-spinner-lite');
        assertTrue(!!spinner);
        assertTrue(spinner.hasAttribute('active'));

        await fakeHandler.whenCalled('checkForIsolatedWebAppUpdate');
        await fakeHandler.flushPipesForTesting();
        await flushTasks();

        // Button is re-enabled and status announces up to date
        assertEquals(null, checkUpdateButton.getAttribute('disabled'));
        const lastCheckText =
            appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
                '.last-check-text');
        assertTrue(!!lastCheckText);
        assertTrue(lastCheckText.innerText.includes('up to date'));
      });

  test(
      'IWA update dialog accessibility structure and cancellation',
      async () => {
        loadTimeData.overrideValues({isIwaInlineUpdateEnabled: true});
        await addApp({
          type: AppType.kWeb,
          publisherId: 'isolated-app://pt2igw6.../',
          version: '1.0.0',
          title: 'Kitchen Sink IWA',
        });

        const checkUpdateButton =
            appDetailsItem.shadowRoot!.querySelector<HTMLElement>(
                '#checkUpdateButton');
        assertTrue(!!checkUpdateButton);

        fakeHandler.updateVersion = {components: [1, 2, 0]};
        fakeHandler.numWindowsForApp = 1;

        checkUpdateButton.click();
        await fakeHandler.whenCalled('checkForIsolatedWebAppUpdate');
        await fakeHandler.whenCalled('getNumWindowsForApp');
        await fakeHandler.flushPipesForTesting();
        await flushTasks();

        // Verify dialog accessibility container and attributes
        const updateFoundDialog =
            appDetailsItem.shadowRoot!.querySelector('#updateFoundDialog');
        assertTrue(!!updateFoundDialog);

        // Verify title and body slots for screen readers
        const titleSlot = updateFoundDialog.querySelector('[slot="title"]');
        assertTrue(!!titleSlot);
        assertEquals('Update found', titleSlot.textContent.trim());

        const bodySlot = updateFoundDialog.querySelector('[slot="body"]');
        assertTrue(!!bodySlot);
        assertTrue(bodySlot.textContent.includes('1.2.0'));
        assertTrue(bodySlot.textContent.includes('Kitchen Sink IWA'));

        // Verify action and cancel buttons
        const cancelButton =
            updateFoundDialog.querySelector<HTMLElement>('#cancelFound');
        assertTrue(!!cancelButton);
        assertEquals('Cancel', cancelButton.innerText.trim());

        const applyButton =
            updateFoundDialog.querySelector<HTMLElement>('#confirmFound');
        assertTrue(!!applyButton);
        assertEquals('Apply update', applyButton.innerText.trim());

        // Test cancelling the dialog closes it without applying update
        cancelButton.click();
        await flushTasks();

        assertNull(
            appDetailsItem.shadowRoot!.querySelector('#updateFoundDialog'));
        assertEquals('Check for updates', checkUpdateButton.innerText.trim());
      });
});
