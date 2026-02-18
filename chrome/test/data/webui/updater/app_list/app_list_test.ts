// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/app_list/app_list.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {AppListElement} from 'chrome://updater/app_list/app_list.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('AppListElement', () => {
  let element: AppListElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      numKnownApps: 1,
      knownAppName0: 'Known App',
      knownAppIds0: 'known-app-id',
    });
  });

  test('displays a message when no apps are installed', async () => {
    element = document.createElement('app-list');
    element.apps = [];
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
    element = document.createElement('app-list');
    element.error = true;
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
    element = document.createElement('app-list');
    element.apps = [
      {
        appId: 'system-app-id',
        version: '1.1.1.1',
        cohort: 'system-cohort',
        scope: 'SYSTEM',
        displayName: 'system-app-id',
      },
      {
        appId: 'known-app-id',
        version: '3.3.3.3',
        cohort: null,
        scope: 'SYSTEM',
        displayName: 'Known App',
      },
      {
        appId: 'user-app-id',
        version: '2.2.2.2',
        cohort: 'user-cohort',
        scope: 'USER',
        displayName: 'user-app-id',
      },
    ];
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
});
