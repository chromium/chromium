// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/app.js';

import type {UpdaterAppElement} from 'chrome://updater/app.js';
import {BrowserProxyImpl} from 'chrome://updater/browser_proxy.js';
import {PageHandlerRemote} from 'chrome://updater/updater_ui.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('UpdaterAppElement', () => {
  let element: UpdaterAppElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;

  async function initApp() {
    element = document.createElement('updater-app');
    document.body.appendChild(element);
    await microtasksFinished();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    BrowserProxyImpl.getInstance().handler = handler;

    handler.setPromiseResolveFor('getAllUpdaterEvents', {events: []});
    handler.setPromiseResolveFor('getUpdaterStates', {
      system: null,
      user: null,
    });
    handler.setPromiseResolveFor('getEnterpriseCompanionState', {
      state: null,
    });
    handler.setPromiseResolveFor('getAppStates', {
      systemApps: [],
      userApps: [],
    });
  });

  test('fetches data on connectedCallback', async () => {
    await initApp();
    assertEquals(1, handler.getCallCount('getAllUpdaterEvents'));
    assertEquals(1, handler.getCallCount('getUpdaterStates'));
    assertEquals(1, handler.getCallCount('getEnterpriseCompanionState'));
    assertEquals(1, handler.getCallCount('getAppStates'));
  });

  test('passes data to sub-components', async () => {
    const systemApp = {
      appId: 'system-app-id',
      version: '1.1.1.1',
      cohort: 'system-cohort',
    };
    handler.setPromiseResolveFor('getAppStates', {
      systemApps: [systemApp],
      userApps: [],
    });
    await initApp();

    const appList = element.shadowRoot.querySelector('app-list');
    assertTrue(!!appList);
    assertEquals(1, appList.apps.length);
    assertEquals('system-app-id', appList.apps[0]!.appId);
  });

  suite('MojoFailure', () => {
    test('handles getUpdaterStates failure', async () => {
      handler.setPromiseRejectFor('getUpdaterStates');
      await initApp();

      assertTrue(element.updaterStateError);
      const updaterState = element.shadowRoot.querySelector('updater-state');
      assertTrue(!!updaterState);
      assertTrue(updaterState.error);
    });

    test('handles getEnterpriseCompanionState failure', async () => {
      handler.setPromiseRejectFor('getEnterpriseCompanionState');
      await initApp();

      assertTrue(element.updaterStateError);
      const updaterState = element.shadowRoot.querySelector('updater-state');
      assertTrue(!!updaterState);
      assertTrue(updaterState.error);
    });

    test('handles getAppStates failure', async () => {
      handler.setPromiseRejectFor('getAppStates');
      await initApp();

      assertTrue(element.appStateError);
      const appList = element.shadowRoot.querySelector('app-list');
      assertTrue(!!appList);
      assertTrue(appList.error);
    });

    test('handles getAllUpdaterEvents failure', async () => {
      handler.setPromiseRejectFor('getAllUpdaterEvents');
      await initApp();

      assertEquals(0, element.messages.length);
      const eventList = element.shadowRoot.querySelector('event-list');
      assertTrue(!!eventList);
      assertEquals(0, eventList.messages.length);
    });
  });
});
