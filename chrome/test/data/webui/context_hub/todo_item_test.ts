// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/taskbox/todo_item.js';

import {browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {SourceReference} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {TodoItemElement} from 'chrome://context-hub/taskbox/todo_item.js';
import {TodoItemVariant} from 'chrome://context-hub/taskbox/todo_item.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createHistoryRef(url: string, title = 'Page'): SourceReference {
  return {
    chromeHistoryEntry: {
      url,
      title,
      visitTimestamp: {internalValue: 0n},
    },
  };
}

function createGmailRef(messageUrl: string, subject = 'Mail'): SourceReference {
  return {
    gmail: {
      messageUrl,
      subject,
    },
  };
}

suite('TodoItem', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let openWindowProxy: TestOpenWindowProxy;
  let item: TodoItemElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: false});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor(
        'openUrlsInTabGroup', Promise.resolve({success: true}));
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  async function createHistoryItem(
      sourceReferences: SourceReference[], heading = 'Plan trip') {
    item = document.createElement('todo-item');
    item.variant = TodoItemVariant.HISTORY;
    item.heading = heading;
    item.sourceReferences = sourceReferences;
    document.body.appendChild(item);
    await microtasksFinished();
  }

  function query<T extends HTMLElement = HTMLElement>(selector: string): T|
      null {
    return item.shadowRoot.querySelector<T>(selector);
  }

  test(
      'continue in tabs opens web source references in a tab group',
      async () => {
        await createHistoryItem([
          createHistoryRef('https://example.com/a', 'A'),
          createHistoryRef('chrome://settings', 'Settings'),
          createGmailRef('http://example.com/b', 'B'),
          createHistoryRef('file:///etc/passwd', 'File'),
        ]);

        const mainButton = query<CrButtonElement>('.split-main-button')!;
        const dropdownButton = query<CrButtonElement>('#taskDropdownButton')!;
        assertFalse(mainButton.disabled);
        assertFalse(dropdownButton.disabled);

        mainButton.click();

        const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
        assertEquals('Plan trip', label);
        assertDeepEquals(
            ['https://example.com/a', 'http://example.com/b'], urls);
        assertEquals(0, openWindowProxy.getCallCount('openUrl'));
      });

  test(
      'continue in tabs menu item opens tab group and closes menu',
      async () => {
        await createHistoryItem([
          createHistoryRef('https://example.com/a', 'A'),
        ]);

        const dropdownButton = query<CrButtonElement>('#taskDropdownButton')!;
        dropdownButton.click();
        await microtasksFinished();

        const taskMenu = query<CrActionMenuElement>('#taskMenu')!;
        assertTrue(taskMenu.open);

        const menuItems =
            taskMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(2, menuItems.length);
        assertFalse(menuItems[0]!.disabled);
        assertTrue(menuItems[1]!.disabled);

        menuItems[0]!.click();

        const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
        await microtasksFinished();
        assertFalse(taskMenu.open);
        assertEquals('Plan trip', label);
        assertDeepEquals(['https://example.com/a'], urls);
      });

  test(
      'continue in tabs uses fallback group label when heading is empty',
      async () => {
        await createHistoryItem(
            [createHistoryRef('https://example.com/a')], /*heading=*/ '');

        query<CrButtonElement>('.split-main-button')!.click();

        const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
        assertEquals('Related Tabs', label);
        assertDeepEquals(['https://example.com/a'], urls);
      });

  test(
      'continue in tabs falls back to opening individual tabs on failure',
      async () => {
        handler.setResultFor(
            'openUrlsInTabGroup', Promise.resolve({success: false}));
        const refs: SourceReference[] = [];
        for (let i = 0; i < 12; i++) {
          refs.push(createHistoryRef(`https://example.com/${i}`));
        }
        await createHistoryItem(refs);

        query<CrButtonElement>('.split-main-button')!.click();

        await handler.whenCalled('openUrlsInTabGroup');
        await microtasksFinished();

        assertEquals(10, openWindowProxy.getCallCount('openUrl'));
        assertDeepEquals(
            Array.from({length: 10}, (_, i) => `https://example.com/${i}`),
            openWindowProxy.getArgs('openUrl'));
      });

  test(
      'continue in tabs is disabled when there are no web references',
      async () => {
        await createHistoryItem([
          createHistoryRef('chrome://history'),
          createHistoryRef('file:///tmp/a'),
        ]);

        const mainButton = query<CrButtonElement>('.split-main-button')!;
        const dropdownButton = query<CrButtonElement>('#taskDropdownButton')!;
        const menuItem = query<HTMLButtonElement>('#taskMenu .dropdown-item')!;
        assertTrue(mainButton.disabled);
        assertTrue(dropdownButton.disabled);
        assertTrue(menuItem.disabled);
      });
});
