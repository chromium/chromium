// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/app_list/app_list.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {AppListElement} from 'chrome://updater/app_list/app_list.js';
import {BrowserProxyImpl} from 'chrome://updater/browser_proxy.js';
import type {GetAppStatesResponse} from 'chrome://updater/updater_ui.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://updater/updater_ui.mojom-webui.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('AppListElement', () => {
  let element: AppListElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;

  const systemApp = {
    appId: 'system-app-id',
    version: '1.1.1.1',
    cohort: 'system-cohort',
  };

  const userApp = {
    appId: 'user-app-id',
    version: '2.2.2.2',
    cohort: 'user-cohort',
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    BrowserProxyImpl.getInstance().handler = handler;

    loadTimeData.overrideValues({
      numKnownApps: 1,
      knownAppName0: 'Known App',
      knownAppIds0: 'known-app-id',
    });
  });

  test('displays a message when no apps are installed', async () => {
    handler.setPromiseResolveFor(
        'getAppStates', {systemApps: [], userApps: []});

    element = document.createElement('app-list');
    document.body.appendChild(element);

    await microtasksFinished();

    const message = element.shadowRoot.querySelector('#no-apps-message');
    assertTrue(!!message);
    assertEquals(
        loadTimeData.getString('noAppsFound'), message.textContent.trim());

    assertFalse(!!element.shadowRoot.querySelector('.error-card'));
    assertFalse(!!element.shadowRoot.querySelector('table'));
  });

  test('displays an error when query fails', async () => {
    handler.setPromiseRejectFor('getAppStates');

    element = document.createElement('app-list');
    document.body.appendChild(element);

    await microtasksFinished();

    const errorCard = element.shadowRoot.querySelector('.error-card');
    assertTrue(!!errorCard);
    assertStringContains(
        errorCard.textContent, loadTimeData.getString('appStatesQueryFailed'));

    assertFalse(!!element.shadowRoot.querySelector('#no-apps-message'));
    assertFalse(!!element.shadowRoot.querySelector('table'));
  });

  test('renders applications', async () => {
    handler.setResultFor('getAppStates', Promise.resolve<GetAppStatesResponse>({
      systemApps: [
        systemApp,
        {appId: 'known-app-id', version: '3.3.3.3', cohort: null},
      ],
      userApps: [userApp],
    }));

    element = document.createElement('app-list');
    document.body.appendChild(element);

    await microtasksFinished();

    assertFalse(!!element.shadowRoot.querySelector('.error-card'));
    assertFalse(!!element.shadowRoot.querySelector('#no-apps-message'));
    const rows = element.shadowRoot.querySelectorAll('tbody tr');
    assertEquals(3, rows.length);

    assertEquals(
        'system-app-id',
        rows[0]!.querySelector('.app-name')!.textContent.trim());
    assertEquals('SYSTEM', rows[0]!.querySelector('scope-icon')!.scope);
    assertEquals(
        '1.1.1.1', rows[0]!.querySelector('.version')!.textContent.trim());

    assertEquals(
        'Known App', rows[1]!.querySelector('.app-name')!.textContent.trim());
    assertEquals('SYSTEM', rows[1]!.querySelector('scope-icon')!.scope);
    assertEquals(
        '3.3.3.3', rows[1]!.querySelector('.version')!.textContent.trim());

    assertEquals(
        'user-app-id', rows[2]!.querySelector('.app-name')!.textContent.trim());
    assertEquals('USER', rows[2]!.querySelector('scope-icon')!.scope);
    assertEquals(
        '2.2.2.2', rows[2]!.querySelector('.version')!.textContent.trim());
  });

  test('handles empty app lists', async () => {
    handler.setResultFor('getAppStates', Promise.resolve<GetAppStatesResponse>({
      systemApps: [],
      userApps: [],
    }));

    element = document.createElement('app-list');
    document.body.appendChild(element);

    await microtasksFinished();

    const rows = element.shadowRoot.querySelectorAll('tbody tr');
    assertEquals(0, rows.length);
  });
});
