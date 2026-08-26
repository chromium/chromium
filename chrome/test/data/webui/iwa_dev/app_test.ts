// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/app.js';

import {getStoredUpdateOptions, getUpdateOptionsStorageKey, MIN_UPDATE_DELAY_MS, saveStoredUpdateOptions} from 'chrome://iwa-dev/app.js';
import type {IwaDevAppElement} from 'chrome://iwa-dev/app.js';
import type {InstalledAppListItemElement} from 'chrome://iwa-dev/installed_app_list_item.js';
import type {IwaDevModeAppInfo, PageCallbackRouter, UpdateManifest, UpdateManifestOptions} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import type {IwaDevUpdateOptionsDialogElement} from 'chrome://iwa-dev/update_options_dialog.js';
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
    window.localStorage.clear();
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

  async function setupManifestInstalledApp(options?: UpdateManifestOptions):
      Promise<IwaDevModeAppInfo> {
    const appInfo = createManifestInstalledAppInfo();
    if (options) {
      saveStoredUpdateOptions(appInfo.appId, options);
    }
    handler.setResultFor(
        'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
    createApp(/*devModeEnabled=*/ true);
    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();
    return appInfo;
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
        handler.setResultFor('updateManifestInstalledApp', Promise.resolve());
        await setupManifestInstalledApp();

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
        handler.setResultFor('updateManifestInstalledApp', Promise.reject({
          message: 'App is already on the latest version.',
        }));
        await setupManifestInstalledApp();

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
    let resolveUpdate!: () => void;
    const updatePromise = new Promise<void>(resolve => {
      resolveUpdate = resolve;
    });
    handler.setResultFor('updateManifestInstalledApp', updatePromise);
    await setupManifestInstalledApp();

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
            dialog.shadowRoot.querySelector<HTMLElement>('.error-message');
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

  test('display update options button only for manifest apps', async () => {
    const apps = [
      createBundleInstalledAppInfo(),
      createProxyInstalledAppInfo(),
      createManifestInstalledAppInfo(),
    ];
    handler.setResultFor('getInstalledAppsInfo', Promise.resolve({apps}));

    createApp(/*devModeEnabled=*/ true);
    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    const items = getListItems();
    assertEquals(3, items.length);

    // Local Bundle App: no update options button
    assertFalse(!!items[0]!.shadowRoot.querySelector('#update-options-btn'));

    // Proxy App: no update options button
    assertFalse(!!items[1]!.shadowRoot.querySelector('#update-options-btn'));

    // Manifest App: update options button present
    const optionsBtn = items[2]!.shadowRoot.querySelector<HTMLButtonElement>(
        '#update-options-btn');
    assertTrue(!!optionsBtn);
  });

  function getUpdateOptionsDialog(): IwaDevUpdateOptionsDialogElement|null {
    return app.shadowRoot.querySelector<IwaDevUpdateOptionsDialogElement>(
        '#updateOptionsDialog');
  }

  test('opens update options dialog on button click', async () => {
    handler.setResultFor(
        'parseUpdateManifestFromUrl',
        Promise.resolve({versions: [], channels: []}));
    await setupManifestInstalledApp();

    const items = getListItems();
    const optionsBtn = items[0]!.shadowRoot.querySelector<HTMLButtonElement>(
        '#update-options-btn')!;
    optionsBtn.click();
    const urlArg = await handler.whenCalled('parseUpdateManifestFromUrl');
    assertEquals('https://example.com/manifest.json', urlArg);
    await microtasksFinished();

    const dialog = getUpdateOptionsDialog()!;
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
  });

  async function testSetUpdateChannel(
      mojoResult: Promise<void>, expectedToast: string) {
    const appInfo = createManifestInstalledAppInfo();
    handler.setResultFor(
        'getInstalledAppsInfo', Promise.resolve({apps: [appInfo]}));
    handler.setResultFor('setUpdateChannel', mojoResult);
    handler.setResultFor(
        'parseUpdateManifestFromUrl',
        Promise.resolve({versions: [], channels: []}));

    createApp(/*devModeEnabled=*/ true);
    await handler.whenCalled('getInstalledAppsInfo');
    await microtasksFinished();

    getListItems()[0]!.dispatchEvent(new CustomEvent('request-update-options', {
      detail: {app: appInfo},
    }));
    await microtasksFinished();

    const dialog = getUpdateOptionsDialog()!;
    assertTrue(!!dialog);
    dialog.dispatchEvent(new CustomEvent('update-options-saved', {
      detail: {app: appInfo, selectedChannel: 'beta'},
    }));

    const [appIdArg, channelArg] = await handler.whenCalled('setUpdateChannel');
    assertEquals(appInfo.appId, appIdArg);
    assertEquals('beta', channelArg);

    await microtasksFinished();
    assertTrue(app.$.toast.open);
    assertEquals(expectedToast, app.$.toast.textContent?.trim());
  }

  test('calls setUpdateChannel on update-options-saved (success)', async () => {
    await testSetUpdateChannel(Promise.resolve(), 'Update options saved');
  });

  test('shows error toast when setUpdateChannel fails', async () => {
    await testSetUpdateChannel(
        Promise.reject({message: 'Channel not found'}),
        'Failed to set update channel: Channel not found');
  });

  test('shows toast when only pinned version is saved', async () => {
    handler.setResultFor(
        'parseUpdateManifestFromUrl',
        Promise.resolve({versions: [], channels: []}));
    const appInfo = await setupManifestInstalledApp();
    getListItems()[0]!.dispatchEvent(new CustomEvent('request-update-options', {
      detail: {app: appInfo},
    }));
    await microtasksFinished();

    const dialog = getUpdateOptionsDialog()!;
    assertTrue(!!dialog);
    dialog.dispatchEvent(new CustomEvent('update-options-saved', {
      detail: {app: appInfo, pinnedVersion: '1.2.0'},
    }));

    await microtasksFinished();
    assertTrue(app.$.toast.open);
    assertEquals('Update options saved', app.$.toast.textContent?.trim());
    assertEquals('1.2.0', getStoredUpdateOptions(appInfo.appId).pinnedVersion);
  });

  test('shows toast when both channel and version are saved', async () => {
    handler.setResultFor('setUpdateChannel', Promise.resolve());
    handler.setResultFor(
        'parseUpdateManifestFromUrl',
        Promise.resolve({versions: [], channels: []}));
    const appInfo = await setupManifestInstalledApp();
    getListItems()[0]!.dispatchEvent(new CustomEvent('request-update-options', {
      detail: {app: appInfo},
    }));
    await microtasksFinished();

    const dialog = getUpdateOptionsDialog()!;
    assertTrue(!!dialog);
    dialog.dispatchEvent(new CustomEvent('update-options-saved', {
      detail: {
        app: appInfo,
        selectedChannel: 'beta',
        pinnedVersion: '1.2.0',
      },
    }));

    const [appIdArg, channelArg] = await handler.whenCalled('setUpdateChannel');
    assertEquals(appInfo.appId, appIdArg);
    assertEquals('beta', channelArg);

    await microtasksFinished();
    assertTrue(app.$.toast.open);
    assertEquals('Update options saved', app.$.toast.textContent?.trim());
    assertEquals('1.2.0', getStoredUpdateOptions(appInfo.appId).pinnedVersion);
  });

  test(
      'calls updateManifestInstalledApp with stored pinned version',
      async () => {
        const appInfo = await setupManifestInstalledApp(
            {allowDowngrades: false, pinnedVersion: '2.5.0'});
        handler.setResultFor('updateManifestInstalledApp', Promise.resolve());

        clickUpdateButton();

        const [appId, options] =
            await handler.whenCalled('updateManifestInstalledApp');
        assertEquals(appInfo.appId, appId);
        assertDeepEquals(
            {allowDowngrades: false, pinnedVersion: '2.5.0'}, options);

        await waitForUpdateCompletion();
      });

  test(
      'passes stored pinned version when opening update options dialog',
      async () => {
        handler.setResultFor(
            'parseUpdateManifestFromUrl',
            Promise.resolve({versions: [], channels: []}));
        const appInfo = await setupManifestInstalledApp(
            {allowDowngrades: false, pinnedVersion: '2.5.0'});

        getListItems()[0]!.dispatchEvent(
            new CustomEvent('request-update-options', {
              detail: {app: appInfo},
            }));
        await microtasksFinished();

        const dialog = getUpdateOptionsDialog()!;
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('2.5.0', dialog.currentPinnedVersion);
      });

  test(
      'removes stored update options when unpinning version on save',
      async () => {
        handler.setResultFor(
            'parseUpdateManifestFromUrl',
            Promise.resolve({versions: [], channels: []}));
        const appInfo = await setupManifestInstalledApp(
            {allowDowngrades: false, pinnedVersion: '1.2.0'});

        getListItems()[0]!.dispatchEvent(
            new CustomEvent('request-update-options', {
              detail: {app: appInfo},
            }));
        await microtasksFinished();

        const dialog = getUpdateOptionsDialog()!;
        assertTrue(!!dialog);
        dialog.dispatchEvent(new CustomEvent('update-options-saved', {
          detail: {app: appInfo, pinnedVersion: null},
        }));

        await microtasksFinished();
        assertTrue(app.$.toast.open);
        assertEquals('Update options saved', app.$.toast.textContent?.trim());
        assertEquals(
            null,
            window.localStorage.getItem(
                getUpdateOptionsStorageKey(appInfo.appId)));
      });

  test('cleans up stored update options on uninstall', async () => {
    const appInfo = await setupManifestInstalledApp(
        {allowDowngrades: false, pinnedVersion: '1.2.0'});
    handler.setResultFor('uninstallApp', Promise.resolve({success: true}));

    getListItems()[0]!.dispatchEvent(
        new CustomEvent('request-uninstall', {detail: {app: appInfo}}));
    await handler.whenCalled('uninstallApp');
    await microtasksFinished();

    assertEquals(
        null,
        window.localStorage.getItem(getUpdateOptionsStorageKey(appInfo.appId)));
  });
});

suite('storage helpers', () => {
  const testAppId = 'test-storage-app-id';

  teardown(() => {
    window.localStorage.removeItem(getUpdateOptionsStorageKey(testAppId));
  });

  test('removes item from localStorage when options match default', () => {
    saveStoredUpdateOptions(
        testAppId, {allowDowngrades: false, pinnedVersion: '1.0.0'});
    assertTrue(
        window.localStorage.getItem(getUpdateOptionsStorageKey(testAppId)) !==
        null);

    saveStoredUpdateOptions(
        testAppId, {allowDowngrades: false, pinnedVersion: null});
    assertEquals(
        null,
        window.localStorage.getItem(getUpdateOptionsStorageKey(testAppId)));
  });

  test('handles corrupted or missing localStorage gracefully', () => {
    window.localStorage.setItem(
        getUpdateOptionsStorageKey(testAppId), 'invalid-json');
    assertDeepEquals(
        {allowDowngrades: false, pinnedVersion: null},
        getStoredUpdateOptions(testAppId));
  });
});
