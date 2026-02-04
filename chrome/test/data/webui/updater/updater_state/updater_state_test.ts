// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/updater_state/updater_state.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://updater/browser_proxy.js';
import type {UpdaterStateElement} from 'chrome://updater/updater_state/updater_state.js';
import type {EnterpriseCompanionState, UpdaterState} from 'chrome://updater/updater_ui.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://updater/updater_ui.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('UpdaterStateElement', () => {
  let element: UpdaterStateElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  const updaterState: UpdaterState = {
    activeVersion: '1.0.0.0',
    inactiveVersions: [],
    lastChecked: new Date('2026-01-02T12:00:00'),
    lastStarted: new Date('2026-01-01T12:00:00'),
    // For simplicity, FilePath is presented as a string instead of a UTF-16
    // byte array (a.k.a. number[]) in these tests by default. It is difficult
    // to detect the correct encoding for the platform; instead, both formats
    // are tested on all platforms.
    installationDirectory: {path: '/path/to/updater'} as unknown as FilePath,
    policies: '{}',
  };
  const enterpriseCompanionState: EnterpriseCompanionState = {
    version: '1.0.0.0',
    installationDirectory: {path: '/path/to/companion'} as unknown as FilePath,
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    BrowserProxyImpl.getInstance().handler = handler;

    handler.setPromiseResolveFor('getEnterpriseCompanionState', {state: null});
  });

  test('renders nothing when no states', async () => {
    handler.setPromiseResolveFor('getUpdaterStates', {
      user: null,
      system: null,
    });

    element = document.createElement('updater-state');
    document.body.appendChild(element);

    await microtasksFinished();

    const message = element.shadowRoot.querySelector('#no-updater-message');
    assertTrue(!!message);
    assertEquals(
        loadTimeData.getString('noUpdaterFound'), message.textContent.trim());

    const cards = element.shadowRoot.querySelectorAll('updater-state-card');
    assertEquals(0, cards.length);

    assertFalse(
        !!element.shadowRoot.querySelector('enterprise-companion-state-card'));
  });

  test('renders user state', async () => {
    handler.setPromiseResolveFor('getUpdaterStates', {
      user: updaterState,
      system: null,
    });

    element = document.createElement('updater-state');
    document.body.appendChild(element);

    await microtasksFinished();

    const message = element.shadowRoot.querySelector('#no-updater-message');
    assertFalse(!!message);

    const cards = element.shadowRoot.querySelectorAll('updater-state-card');
    assertEquals(1, cards.length);
    assertEquals('USER', cards[0]!.scope);
  });

  test('renders system state', async () => {
    handler.setPromiseResolveFor('getUpdaterStates', {
      user: null,
      system: updaterState,
    });

    element = document.createElement('updater-state');
    document.body.appendChild(element);

    await microtasksFinished();

    const message = element.shadowRoot.querySelector('#no-updater-message');
    assertFalse(!!message);

    const cards = element.shadowRoot.querySelectorAll('updater-state-card');
    assertEquals(1, cards.length);
    assertEquals('SYSTEM', cards[0]!.scope);
  });

  test('renders both states', async () => {
    handler.setPromiseResolveFor(
        'getUpdaterStates', {user: updaterState, system: updaterState});

    element = document.createElement('updater-state');
    document.body.appendChild(element);

    await microtasksFinished();

    const message = element.shadowRoot.querySelector('#no-updater-message');
    assertFalse(!!message);

    const cards = element.shadowRoot.querySelectorAll('updater-state-card');
    assertEquals(2, cards.length);
    assertEquals('SYSTEM', cards[0]!.scope);
    assertEquals('USER', cards[1]!.scope);
  });

  test('renders enterprise companion state', async () => {
    handler.setPromiseResolveFor('getUpdaterStates', {
      user: null,
      system: null,
    });
    handler.setPromiseResolveFor(
        'getEnterpriseCompanionState', {state: enterpriseCompanionState});

    element = document.createElement('updater-state');
    document.body.appendChild(element);

    await microtasksFinished();

    assertFalse(!!element.shadowRoot.querySelector('#no-updater-message'));

    const card =
        element.shadowRoot.querySelector('enterprise-companion-state-card');
    assertTrue(!!card);
    assertEquals('1.0.0.0', card.version);
    assertEquals('/path/to/companion', card.installPath);
  });

  test('renders error message when updater state query fails', async () => {
    handler.setPromiseRejectFor('getUpdaterStates');

    element = document.createElement('updater-state');
    document.body.appendChild(element);

    await microtasksFinished();

    const errorMessage = element.shadowRoot.querySelector('#error-message');
    assertTrue(!!errorMessage);
    assertEquals(
        loadTimeData.getString('updaterStateQueryFailed'),
        errorMessage.textContent.trim());

    assertFalse(!!element.shadowRoot.querySelector('#no-updater-message'));
    assertEquals(
        0, element.shadowRoot.querySelectorAll('updater-state-card').length);
  });

  test(
      'renders error message when enterprise companion state query fails',
      async () => {
        handler.setPromiseResolveFor('getUpdaterStates', {
          user: null,
          system: null,
        });
        handler.setPromiseRejectFor('getEnterpriseCompanionState');

        element = document.createElement('updater-state');
        document.body.appendChild(element);

        await microtasksFinished();

        const errorMessage = element.shadowRoot.querySelector('#error-message');
        assertTrue(!!errorMessage);
        assertEquals(
            loadTimeData.getString('updaterStateQueryFailed'),
            errorMessage.textContent.trim());

        assertFalse(!!element.shadowRoot.querySelector('#no-updater-message'));
        assertEquals(
            0,
            element.shadowRoot.querySelectorAll('updater-state-card').length);
      });

  suite('provides installation directory', () => {
    test('from string', async () => {
      handler.setPromiseResolveFor('getUpdaterStates', {
        user: null,
        system: updaterState,
      });

      element = document.createElement('updater-state');
      document.body.appendChild(element);

      await microtasksFinished();

      const cards = element.shadowRoot.querySelectorAll('updater-state-card');
      assertEquals(1, cards.length);
      assertEquals('/path/to/updater', cards[0]!.installPath);
    });

    test('from UTF-16 byte array', async () => {
      handler.setPromiseResolveFor('getUpdaterStates', {
        user: null,
        system: {
          ...updaterState,
          installationDirectory: {path: [55357, 56960]} as unknown as FilePath,
        },
      });

      element = document.createElement('updater-state');
      document.body.appendChild(element);

      await microtasksFinished();

      const cards = element.shadowRoot.querySelectorAll('updater-state-card');
      assertEquals(1, cards.length);
      assertEquals('🚀', cards[0]!.installPath);
    });
  });
});
