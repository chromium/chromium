// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/app.js';

import {MIN_UPDATE_DELAY_MS} from 'chrome://iwa-dev/app.js';
import type {IwaDevAppElement} from 'chrome://iwa-dev/app.js';
import type {InstalledAppListItemElement} from 'chrome://iwa-dev/installed_app_list_item.js';
import type {IwaDevModeAppInfo, PageCallbackRouter, UpdateManifest} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-app>', () => {
  let app: IwaDevAppElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  let installedListener: ((appInfo: IwaDevModeAppInfo) => void)|undefined =
      undefined;
  let updatedListener: ((appInfo: IwaDevModeAppInfo) => void)|undefined =
      undefined;
  let uninstalledListener: ((appId: string) => void)|undefined = undefined;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    browserProxyFactory.setInstance({
      handler: handler,
      callbackRouter: {
        onAppInstalled: {
          addListener: (listener: (appInfo: IwaDevModeAppInfo) => void) => {
            installedListener = listener;
            return 1;
          },
        },
        onAppUpdated: {
          addListener: (listener: (appInfo: IwaDevModeAppInfo) => void) => {
            updatedListener = listener;
            return 2;
          },
        },
        onAppUninstalled: {
          addListener: (listener: (appId: string) => void) => {
            uninstalledListener = listener;
            return 3;
          },
        },
        removeListener: () => {},
      } as unknown as PageCallbackRouter,
    });
  });

  teardown(() => {
    installedListener = undefined;
    updatedListener = undefined;
    uninstalledListener = undefined;
  });

  function createApp(devModeEnabled: boolean = true) {
    loadTimeData.overrideValues({isIwaDevModeEnabled: devModeEnabled});
    app = document.createElement('iwa-dev-app');
    document.body.appendChild(app);
  }

  function createBundleInstalledAppInfo(): IwaDevModeAppInfo {
    return {
      appId: 'test-bundle-app-id',
      webBundleId: 'test-bundle-id',
      name: 'Test Bundle App',
      installedVersion: '1.0.0',
      source: {
        bundlePath: {
          path: '/path/to/app.swbn',
        },
      } as unknown as IwaDevModeAppInfo['source'],
    };
  }

  function createProxyInstalledAppInfo(): IwaDevModeAppInfo {
    return {
      appId: 'test-app-id',
      webBundleId: 'test-bundle-id',
      name: 'Test App',
      installedVersion: '1.0.0',
      source: {
        proxyOrigin: {
          scheme: 'https',
          host: 'example.com',
          port: 443,
          nonceIfOpaque: null,
        },
      },
    };
  }

  function createManifestInstalledAppInfo(): IwaDevModeAppInfo {
    return {
      appId: 'test-manifest-app-id',
      webBundleId: 'test-bundle-id',
      name: 'Test Manifest App',
      installedVersion: '1.0.0',
      source: {
        updateInfo: {
          updateManifestUrl: 'https://example.com/manifest.json',
          updateChannel: 'default',
        },
      } as unknown as IwaDevModeAppInfo['source'],
    };
  }

  function getListItems(): NodeListOf<InstalledAppListItemElement> {
    return app.shadowRoot.querySelectorAll<InstalledAppListItemElement>(
        'installed-app-list-item');
  }

  function getUpdateButton(itemIndex: number = 0): HTMLButtonElement {
    const items = getListItems();
    assertTrue(items.length > itemIndex);
    const updateButton =
        items[itemIndex]!.shadowRoot.querySelector<HTMLButtonElement>(
            '#update-btn');
    assertTrue(!!updateButton);
    return updateButton;
  }

  function clickUpdateButton(itemIndex: number = 0) {
    getUpdateButton(itemIndex).click();
  }

  async function waitForUpdateCompletion() {
    await new Promise(resolve => setTimeout(resolve, MIN_UPDATE_DELAY_MS + 10));
    await microtasksFinished();
  }

  test('display error message when IWA dev mode is disabled', async () => {
    createApp(/*devModeEnabled=*/ false);
    await microtasksFinished();

    const heading = app.shadowRoot.querySelector('h1');
    assertTrue(!!heading);
    assertEquals('Isolated Web App Developer Tool', heading.textContent.trim());

    const errorMessage = app.shadowRoot.querySelector('#error-message');
    assertTrue(!!errorMessage);
    assertTrue(errorMessage.textContent.includes(
        'Isolated Web App Developer Mode is disabled.'));
    const link = errorMessage.querySelector('a');
    assertTrue(!!link);
    assertEquals('chrome://flags/#enable-isolated-web-app-dev-mode', link.href);
  });

  test('display content when IWA dev mode is enabled', async () => {
    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps: []}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const heading = app.shadowRoot.querySelector('h1');
    assertTrue(!!heading);
    assertEquals('Isolated Web App Developer Tool', heading.textContent.trim());

    assertFalse(!!app.shadowRoot.querySelector('#error-message'));
  });

  test('display message when no IWAs installed', async () => {
    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps: []}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const message = app.shadowRoot.querySelector('#iwa-list-message');
    assertTrue(!!message);
    assertTrue(message.textContent.includes(
        'No Isolated Web Apps installed in developer mode.'));
  });

  test('display list of installed apps', async () => {
    const apps: IwaDevModeAppInfo[] = [
      {
        appId: 'test-app-id',
        webBundleId: 'test-bundle-id',
        name: 'Test App',
        installedVersion: '1.0.0',
        source: {
          proxyOrigin: {
            scheme: 'https',
            host: 'example.com',
            port: 443,
            nonceIfOpaque: null,
          },
        },
      },
    ];

    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const items = getListItems();
    assertEquals(1, items.length);
  });

  test('updates list when app is installed', async () => {
    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps: []}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    let items = getListItems();
    assertEquals(0, items.length);

    assertTrue(installedListener !== undefined);
    const appInfo = createProxyInstalledAppInfo();
    installedListener(appInfo);

    await microtasksFinished();

    items = getListItems();
    assertEquals(1, items.length);
    assertEquals(appInfo, items[0]!.app);
  });

  test('updates list when app is updated', async () => {
    const appInfo = createProxyInstalledAppInfo();
    handler.setResultFor(
        'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    let items = getListItems();
    assertEquals(1, items.length);
    assertEquals(appInfo, items[0]!.app);

    assertTrue(updatedListener !== undefined);

    const updatedAppInfo = {...appInfo, installedVersion: '2.0.0'};
    updatedListener(updatedAppInfo);

    await microtasksFinished();

    items = getListItems();
    assertEquals(1, items.length);
    assertEquals(updatedAppInfo, items[0]!.app);
  });

  test('updates list when app is uninstalled', async () => {
    handler.setResultFor(
        'getInstalledAppsInfo',
        Promise.resolve({apps: [createProxyInstalledAppInfo()]}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    let items = getListItems();
    assertEquals(1, items.length);

    assertTrue(uninstalledListener !== undefined);
    uninstalledListener('test-app-id');

    await microtasksFinished();

    items = getListItems();
    assertEquals(0, items.length);
  });

  test('calls uninstallApp when uninstall button is clicked', async () => {
    const appInfo = createProxyInstalledAppInfo();
    handler.setResultFor(
        'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const items = getListItems();
    assertEquals(1, items.length);

    const uninstallButton =
        items[0]!.shadowRoot.querySelector<HTMLElement>('#uninstall-btn');
    assertTrue(!!uninstallButton);
    uninstallButton.click();

    const appId = await handler.whenCalled('uninstallApp');
    assertEquals('test-app-id', appId);
  });

  test(
      'calls selectAndUpdateAppFromLocalWebBundle on update click for ' +
          'local bundle app (success)',
      async () => {
        const appInfo = createBundleInstalledAppInfo();
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
        handler.setResultFor(
            'selectAndUpdateAppFromLocalWebBundle', Promise.resolve());
        createApp(/*devModeEnabled=*/ true);

        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        assertEquals(1, getListItems().length);

        clickUpdateButton();

        const appId =
            await handler.whenCalled('selectAndUpdateAppFromLocalWebBundle');
        assertEquals('test-bundle-app-id', appId);

        await waitForUpdateCompletion();
        assertTrue(app.$.toast.open);
        assertEquals('Update successful!', app.$.toast.textContent?.trim());
      });

  test(
      'calls selectAndUpdateAppFromLocalWebBundle on update click for ' +
          'local bundle app (error)',
      async () => {
        const appInfo = createBundleInstalledAppInfo();
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
        handler.setResultFor(
            'selectAndUpdateAppFromLocalWebBundle',
            Promise.reject({message: 'User cancelled'}));
        createApp(/*devModeEnabled=*/ true);

        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        assertEquals(1, getListItems().length);

        clickUpdateButton();

        await handler.whenCalled('selectAndUpdateAppFromLocalWebBundle');
        await waitForUpdateCompletion();

        assertTrue(app.$.toast.open);
        assertEquals(
            'Update failed: User cancelled', app.$.toast.textContent?.trim());
      });

  test(
      'calls updateDevProxyInstalledApp on update click for ' +
          'proxy app (success)',
      async () => {
        const appInfo = createProxyInstalledAppInfo();
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
        handler.setResultFor('updateDevProxyInstalledApp', Promise.resolve());
        createApp(/*devModeEnabled=*/ true);

        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        assertEquals(1, getListItems().length);

        clickUpdateButton();

        const appId = await handler.whenCalled('updateDevProxyInstalledApp');
        assertEquals('test-app-id', appId);

        await waitForUpdateCompletion();
        assertTrue(app.$.toast.open);
        assertEquals('Update successful!', app.$.toast.textContent?.trim());
      });

  test(
      'calls updateDevProxyInstalledApp on update click for ' +
          'proxy app (error)',
      async () => {
        const appInfo = createProxyInstalledAppInfo();
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
        handler.setResultFor(
            'updateDevProxyInstalledApp',
            Promise.reject({message: 'Network error'}));
        createApp(/*devModeEnabled=*/ true);

        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        assertEquals(1, getListItems().length);

        clickUpdateButton();

        await handler.whenCalled('updateDevProxyInstalledApp');
        await waitForUpdateCompletion();

        assertTrue(app.$.toast.open);
        assertEquals(
            'Update failed: Network error', app.$.toast.textContent?.trim());
      });

  test(
      'calls updateManifestInstalledApp on update click for ' +
          'manifest app (success)',
      async () => {
        const appInfo = createManifestInstalledAppInfo();
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
        handler.setResultFor('updateManifestInstalledApp', Promise.resolve());
        createApp(/*devModeEnabled=*/ true);

        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        assertEquals(1, getListItems().length);

        clickUpdateButton();

        const [appId, options] =
            await handler.whenCalled('updateManifestInstalledApp');
        assertEquals('test-manifest-app-id', appId);
        assertDeepEquals(
            {allowDowngrades: false, pinnedVersion: null}, options);

        await waitForUpdateCompletion();
        assertTrue(app.$.toast.open);
        assertEquals('Update successful!', app.$.toast.textContent?.trim());
      });

  test(
      'calls updateManifestInstalledApp on update click for ' +
          'manifest app (error)',
      async () => {
        const appInfo = createManifestInstalledAppInfo();
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
        handler.setResultFor('updateManifestInstalledApp', Promise.reject({
          message: 'App is already on the latest version.',
        }));
        createApp(/*devModeEnabled=*/ true);

        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        assertEquals(1, getListItems().length);

        clickUpdateButton();

        await handler.whenCalled('updateManifestInstalledApp');
        await waitForUpdateCompletion();

        assertTrue(app.$.toast.open);
        assertEquals(
            'Update failed: App is already on the latest version.',
            app.$.toast.textContent?.trim());
      });

  test('disables update button while update is in progress', async () => {
    const appInfo = createManifestInstalledAppInfo();
    handler.setResultFor(
        'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));

    let resolveUpdate!: () => void;
    const updatePromise = new Promise<void>(resolve => {
      resolveUpdate = resolve;
    });
    handler.setResultFor('updateManifestInstalledApp', updatePromise);

    createApp(/*devModeEnabled=*/ true);

    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    assertFalse(
        getUpdateButton().disabled, 'Button should be enabled initially');

    clickUpdateButton();
    await microtasksFinished();

    assertTrue(
        getUpdateButton().disabled, 'Button should be disabled after click');

    resolveUpdate();
    await waitForUpdateCompletion();

    assertFalse(
        getUpdateButton().disabled,
        'Button should be enabled after update completes');
  });

  test('opens install dialog on install button click', async () => {
    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps: []}));

    createApp(/*devModeEnabled=*/ true);
    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const dialog = app.$.installDialog;
    assertTrue(!!dialog);

    const crDialog = dialog.$.dialog;
    assertTrue(!!crDialog);
    assertFalse(crDialog.open);

    const installButton = app.$.installButton;
    assertTrue(!!installButton);
    installButton.click();
    await microtasksFinished();

    assertTrue(crDialog.open);
  });

  test(
      'calls installAppFromDevProxy when dialog requests ' +
          'install from dev proxy',
      async () => {
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: []}));
        handler.setResultFor('installAppFromDevProxy', Promise.resolve());

        createApp(/*devModeEnabled=*/ true);
        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        const dialog = app.$.installDialog;
        assertTrue(!!dialog);

        const devProxyUrl = 'http://localhost:8080';
        dialog.dispatchEvent(new CustomEvent('request-install-from-dev-proxy', {
          detail: {url: devProxyUrl},
        }));
        const url = await handler.whenCalled('installAppFromDevProxy');
        assertEquals(devProxyUrl, url);

        await microtasksFinished();
        assertTrue(app.$.toast.open);
        assertEquals(
            'Installation successful!', app.$.toast.textContent?.trim());
      });

  test(
      'calls selectAndInstallAppFromLocalWebBundle when dialog requests ' +
          'install from local bundle',
      async () => {
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: []}));
        handler.setResultFor(
            'selectAndInstallAppFromLocalWebBundle', Promise.resolve());

        createApp(/*devModeEnabled=*/ true);
        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        const dialog = app.$.installDialog;
        assertTrue(!!dialog);

        dialog.dispatchEvent(
            new CustomEvent('request-install-from-local-bundle'));
        await handler.whenCalled('selectAndInstallAppFromLocalWebBundle');
        await microtasksFinished();

        const crDialog = dialog.$.dialog;
        assertTrue(!!crDialog);
        assertFalse(crDialog.open);
        assertTrue(app.$.toast.open);
        assertEquals(
            'Installation successful!', app.$.toast.textContent?.trim());
      });

  test(
      'displays error when local bundle installation is cancelled',
      async () => {
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: []}));
        handler.setResultFor(
            'selectAndInstallAppFromLocalWebBundle',
            Promise.reject({message: 'No file selected'}));

        createApp(/*devModeEnabled=*/ true);
        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        const dialog = app.$.installDialog;
        assertTrue(!!dialog);

        dialog.dispatchEvent(
            new CustomEvent('request-install-from-local-bundle'));
        await handler.whenCalled('selectAndInstallAppFromLocalWebBundle');
        await microtasksFinished();

        const errorDiv =
            dialog.shadowRoot.querySelector<HTMLElement>('#error-message');
        assertTrue(!!errorDiv);
        assertEquals('No file selected', errorDiv.textContent?.trim());
      });

  async function testParseUpdateManifestFromUrl(
      mojoResult: Promise<UpdateManifest>):
      Promise<{success?: UpdateManifest, error?: string}> {
    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps: []}));
    handler.setResultFor('parseUpdateManifestFromUrl', mojoResult);

    createApp(/*devModeEnabled=*/ true);
    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const testUrl = 'https://example.com/manifest.json';
    let callbackResult: {success?: UpdateManifest, error?: string}|undefined;

    app.$.installDialog.dispatchEvent(
        new CustomEvent('request-parse-update-manifest-from-url', {
          detail: {
            url: testUrl,
            callback: (result: {success?: UpdateManifest, error?: string}) => {
              callbackResult = result;
            },
          },
        }));

    const urlArg = await handler.whenCalled('parseUpdateManifestFromUrl');
    assertEquals(testUrl, urlArg);

    await microtasksFinished();
    assertTrue(callbackResult !== undefined);
    return callbackResult;
  }

  test(
      'calls parseUpdateManifestFromUrl when dialog requests ' +
          'parse update manifest from url (success)',
      async () => {
        const mockManifest: UpdateManifest = {
          versions: [{
            version: '1.0.0',
            src: 'https://example.com/bundle.swbn',
            channels: ['stable'],
          }],
          channels: [{channel: 'stable', displayName: 'Stable'}],
        };
        const result =
            await testParseUpdateManifestFromUrl(Promise.resolve(mockManifest));
        assertEquals(mockManifest, result.success);
      });

  test(
      'calls parseUpdateManifestFromUrl when dialog requests ' +
          'parse update manifest from url (error)',
      async () => {
        const errorMessage = 'Manifest fetch failed: 404 Not Found';
        const result =
            await testParseUpdateManifestFromUrl(Promise.reject(errorMessage));
        assertEquals(errorMessage, result.error);
      });

  test(
      'calls installAppFromUpdateManifest when dialog requests ' +
          'install from update manifest',
      async () => {
        handler.setResultFor(
            'getInstalledAppsInfo', Promise.resolve({apps: []}));
        handler.setResultFor('installAppFromUpdateManifest', Promise.resolve());

        createApp(/*devModeEnabled=*/ true);
        await handler.whenCalled('getInstalledAppsInfo');
        await microtasksFinished();

        const webBundleUrl = 'http://localhost:8080/app.swbn';
        const updateInfo = {
          updateManifestUrl: 'http://localhost:8080/manifest.json',
          updateChannel: 'stable',
        };
        app.$.installDialog.dispatchEvent(
            new CustomEvent('request-install-from-update-manifest', {
              detail: {webBundleUrl, updateInfo},
            }));
        const [bundleUrlArg, updateInfoArg] =
            await handler.whenCalled('installAppFromUpdateManifest');
        assertEquals(webBundleUrl, bundleUrlArg);
        assertDeepEquals(updateInfo, updateInfoArg);

        await microtasksFinished();
        assertTrue(app.$.toast.open);
        assertEquals(
            'Installation successful!', app.$.toast.textContent?.trim());
      });
});
