// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/app.js';

import type {ContextHubAppElement} from 'chrome://context-hub/app.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {CrMenuSelectorElement} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextHubApp', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let app: ContextHubAppElement;

  function resetUrlAndStorage() {
    window.history.replaceState(
        null, '', window.location.pathname + window.location.search);
    localStorage.clear();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetUrlAndStorage();
    loadTimeData.overrideValues({kAutoTodos: false, kLaunchpad: false});
    handler = TestMock.fromClass(PageHandlerRemote);
    handler.setResultFor(
        'getAllMemoryBankEntries', Promise.resolve({entries: []}));
    handler.setResultFor(
        'getMemoryBankChatHistory', Promise.resolve({history: []}));
    handler.setResultFor('getAutoTodos', Promise.resolve({
      firstPartyTodos: [],
      thirdPartyTodos: [],
      firstPartyMetadata: {lastGenerationTime: null, hasError: false},
      thirdPartyMetadata: {lastGenerationTime: null, hasError: false},
    }));
    handler.setResultFor('getTasks', Promise.resolve({tasks: []}));
    handler.setResultFor('getTodoFeedbacks', Promise.resolve({feedbacks: []}));
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
  });

  teardown(() => {
    resetUrlAndStorage();
  });

  async function createApp() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('context-hub-app');
    document.body.appendChild(app);
    await microtasksFinished();
  }

  function getSelector(): CrMenuSelectorElement {
    return app.shadowRoot.querySelector<CrMenuSelectorElement>('#selector')!;
  }

  test(
      'hides launchpad and defaults to memory-banks when disabled',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: false, kLaunchpad: false});
        await createApp();

        assertNull(app.shadowRoot.querySelector('a[data-route="launchpad"]'));
        assertEquals('memory-banks', getSelector().selected);
        assertTrue(!!app.shadowRoot.querySelector('memory-banks'));
        assertNull(app.shadowRoot.querySelector('ai-taskbox'));
      });

  test('falls back to memory-banks when launchpad is disabled', async () => {
    loadTimeData.overrideValues({kAutoTodos: false, kLaunchpad: false});

    window.location.hash = '#launchpad';
    await createApp();
    assertEquals('memory-banks', getSelector().selected);
    assertTrue(!!app.shadowRoot.querySelector('memory-banks'));
    assertNull(app.shadowRoot.querySelector('ai-taskbox'));

    window.location.hash = '#ai-taskbox';
    await createApp();
    assertEquals('memory-banks', getSelector().selected);
    assertTrue(!!app.shadowRoot.querySelector('memory-banks'));
    assertNull(app.shadowRoot.querySelector('ai-taskbox'));

    resetUrlAndStorage();
    localStorage.setItem('context_hub_current_view', 'launchpad');
    await createApp();
    assertEquals('memory-banks', getSelector().selected);
    assertTrue(!!app.shadowRoot.querySelector('memory-banks'));
    assertNull(app.shadowRoot.querySelector('ai-taskbox'));
  });

  test('defaults to launchpad when enabled after disabled visit', async () => {
    loadTimeData.overrideValues({kAutoTodos: false, kLaunchpad: false});
    await createApp();
    assertEquals('memory-banks', getSelector().selected);
    assertNull(localStorage.getItem('context_hub_current_view'));
    assertEquals('', window.location.hash);

    loadTimeData.overrideValues({kAutoTodos: true, kLaunchpad: false});
    await createApp();
    assertTrue(!!app.shadowRoot.querySelector('a[data-route="launchpad"]'));
    assertEquals('launchpad', getSelector().selected);
    assertTrue(!!app.shadowRoot.querySelector('ai-taskbox'));
    assertNull(app.shadowRoot.querySelector('memory-banks'));
  });

  test(
      'shows launchpad and defaults to it when kLaunchpad alone is enabled',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: false, kLaunchpad: true});
        await createApp();

        assertTrue(!!app.shadowRoot.querySelector('a[data-route="launchpad"]'));
        assertEquals('launchpad', getSelector().selected);
        assertTrue(!!app.shadowRoot.querySelector('ai-taskbox'));
        assertNull(app.shadowRoot.querySelector('memory-banks'));
        assertEquals(1, handler.getCallCount('getTasks'));
        assertEquals(0, handler.getCallCount('getAutoTodos'));
      });
});
