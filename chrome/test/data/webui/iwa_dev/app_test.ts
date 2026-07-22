// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/app.js';

import type {IwaDevAppElement} from 'chrome://iwa-dev/app.js';
import type {IwaDevModeAppInfo} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-app>', () => {
  let app: IwaDevAppElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    browserProxyFactory.setInstance({handler});
  });

  function createApp(devModeEnabled: boolean = true) {
    loadTimeData.overrideValues({isIwaDevModeEnabled: devModeEnabled});
    app = document.createElement('iwa-dev-app');
    document.body.appendChild(app);
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

    const items = app.shadowRoot.querySelectorAll('installed-app-list-item');
    assertEquals(1, items.length);
  });
});
