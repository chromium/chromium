// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/app.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PageDataSource} from 'chrome://updater/app.js';
import type {UpdaterAppElement} from 'chrome://updater/app.js';
import {BrowserProxyImpl} from 'chrome://updater/browser_proxy.js';
import {PageHandlerRemote} from 'chrome://updater/updater_ui.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('UpdaterAppElement', () => {
  let element: UpdaterAppElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;

  async function initApp() {
    element = document.createElement('updater-app');
    document.body.appendChild(element);
    await microtasksFinished();
  }

  async function setInputFile(filename: string, content: string) {
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(new File([content], filename));
    element.$.fileInput.files = dataTransfer.files;
    element.$.fileInput.dispatchEvent(new Event('change'));
    await microtasksFinished();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    BrowserProxyImpl.getInstance().handler = handler;

    PluralStringProxyImpl.setInstance(new TestPluralStringProxy());

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

  suite('presents external data', () => {
    const events = [
      JSON.stringify({
        eventType: 'UPDATER_PROCESS',
        eventId: '1',
        deviceUptime: 1000,
        pid: 123,
        processToken: 'token',
        bound: 'START',
        scope: 'SYSTEM',
        updaterVersion: '123.0.0.1',
        timestamp: '13351910400000000',
      }),
      JSON.stringify({
        eventType: 'UPDATER_PROCESS',
        eventId: '1',
        deviceUptime: 2000,
        pid: 123,
        processToken: 'token',
        bound: 'END',
      }),
      JSON.stringify({
        eventType: 'PERSISTED_DATA',
        eventId: '2',
        deviceUptime: 1500,
        pid: 123,
        processToken: 'token',
        bound: 'INSTANT',
        eulaRequired: false,
        registeredApps: [{appId: 'test-app', version: '1.0.0.0'}],
      }),
    ].join('\n');

    test(
        'switches to file data source on processHistoryFiles success',
        async () => {
          await initApp();
          assertEquals(PageDataSource.INSTALL, element.pageDataSource);

          await setInputFile('history.jsonl', events);

          assertEquals(PageDataSource.FILE, element.pageDataSource);
          assertFalse(element.historyLoadError);
          assertEquals(3, element.messages.length);
          assertEquals(1, element.apps.length);
          assertEquals('test-app', element.apps[0]!.appId);

          const closeButton = element.shadowRoot.querySelector<HTMLElement>(
              '#controls cr-button');
          assertTrue(!!closeButton);
          closeButton.click();
          await microtasksFinished();

          assertEquals(PageDataSource.INSTALL, element.pageDataSource);
        });

    test(
        'switches to file data source on unzipUpdaterHistoryFiles success',
        async () => {
          await initApp();
          assertEquals(PageDataSource.INSTALL, element.pageDataSource);

          handler.setPromiseResolveFor('unzipUpdaterHistoryFiles', {
            historyFileContents: [events],
          });

          await setInputFile('history.zip', 'zip content');

          assertEquals(PageDataSource.FILE, element.pageDataSource);
          assertFalse(element.historyLoadError);
          assertEquals(3, element.messages.length);
          assertEquals(1, element.apps.length);
          assertEquals('test-app', element.apps[0]!.appId);

          const closeButton = element.shadowRoot.querySelector<HTMLElement>(
              '#controls cr-button');
          assertTrue(!!closeButton);
          closeButton.click();
          await microtasksFinished();

          assertEquals(PageDataSource.INSTALL, element.pageDataSource);
        });

    test('handles unzipUpdaterHistoryFiles failure', async () => {
      await initApp();

      handler.setPromiseRejectFor('unzipUpdaterHistoryFiles');

      await setInputFile('history.zip', 'zip content');

      assertEquals(1, handler.getCallCount('unzipUpdaterHistoryFiles'));
      assertEquals(PageDataSource.INSTALL, element.pageDataSource);
      assertTrue(element.historyLoadError);
    });

    test('handles invalid file extension', async () => {
      await initApp();

      await setInputFile('invalid.txt', 'some data');

      assertEquals(PageDataSource.INSTALL, element.pageDataSource);
      assertTrue(element.historyLoadError);
    });

    test('handles invalid JSON', async () => {
      await initApp();

      await setInputFile('invalid.jsonl', 'not json');

      assertEquals(PageDataSource.INSTALL, element.pageDataSource);
      assertTrue(element.historyLoadError);
    });
  });
});
