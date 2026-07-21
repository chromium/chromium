// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://web-app-internals/app.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import type {WebAppInternalsAppElement} from 'chrome://web-app-internals/app.js';
import {browserProxyFactory} from 'chrome://web-app-internals/web_app_internals.mojom-webui.js';
import type {WebAppInternalsHandlerInterface} from 'chrome://web-app-internals/web_app_internals.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestWebAppInternalsHandler extends TestBrowserProxy implements
    WebAppInternalsHandlerInterface {
  private debugInfoJson_: string = '';

  constructor() {
    super(['getDebugInfoAsJsonString']);
  }

  setDebugInfoJson(json: string) {
    this.debugInfoJson_ = json;
  }

  getDebugInfoAsJsonString(): Promise<{result: string}> {
    this.methodCalled('getDebugInfoAsJsonString');
    return Promise.resolve({result: this.debugInfoJson_});
  }

  installIsolatedWebAppFromDevProxy() {
    return assertNotReached();
  }

  selectFileAndInstallIsolatedWebAppFromDevBundle() {
    return assertNotReached();
  }

  parseUpdateManifestFromUrl() {
    return assertNotReached();
  }

  installIsolatedWebAppFromBundleUrl() {
    return assertNotReached();
  }

  updateDevProxyIsolatedWebApp() {
    return assertNotReached();
  }

  selectFileAndUpdateIsolatedWebAppFromDevBundle() {
    return assertNotReached();
  }

  updateManifestInstalledIsolatedWebApp() {
    return assertNotReached();
  }

  deleteIsolatedWebApp() {
    return assertNotReached();
  }

  setUpdateChannelForIsolatedWebApp() {
    return assertNotReached();
  }

  setPinnedVersionForIsolatedWebApp() {
    return assertNotReached();
  }

  resetPinnedVersionForIsolatedWebApp() {
    return assertNotReached();
  }

  setAllowDowngradesForIsolatedWebApp() {
    return assertNotReached();
  }

  searchForIsolatedWebAppUpdates() {
    return assertNotReached();
  }

  getIsolatedWebAppDevModeAppInfo() {
    return assertNotReached();
  }
}

suite('WebAppInternalsAppElementTest', function() {
  let element: WebAppInternalsAppElement;
  let handler: TestWebAppInternalsHandler;

  const fakeData = {
    InstalledWebApps: {
      '!Index': {'App1': 'id1', 'App2': 'id2'},
      Details: [],
    },
  };

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.location.hash = '';

    handler = new TestWebAppInternalsHandler();
    handler.setDebugInfoJson(JSON.stringify(fakeData));
    browserProxyFactory.setInstance({handler});

    element = document.createElement('web-app-internals-app');
    document.body.appendChild(element);
    await handler.whenCalled('getDebugInfoAsJsonString');
    await microtasksFinished();
  });

  test('renders app index correctly and updates active link', async function() {
    const links =
        element.shadowRoot.querySelectorAll<HTMLLinkElement>('#app-index a');
    assertEquals(3, links.length);

    assertEquals('Show All', links[0]!.textContent.trim());
    assertEquals('App1 (id1)', links[1]!.textContent.trim());
    assertEquals('App2 (id2)', links[2]!.textContent.trim());

    assertTrue(links[0]!.classList.contains('active'));
    assertFalse(links[1]!.classList.contains('active'));
    assertFalse(links[2]!.classList.contains('active'));

    window.location.hash = '#id1';
    window.dispatchEvent(new HashChangeEvent('hashchange'));
    await microtasksFinished();

    assertFalse(links[0]!.classList.contains('active'));
    assertTrue(links[1]!.classList.contains('active'));
    assertFalse(links[2]!.classList.contains('active'));
  });
});
