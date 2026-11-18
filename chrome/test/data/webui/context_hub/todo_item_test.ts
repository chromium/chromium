// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/taskbox/ai_taskbox.js';
import 'chrome://context-hub/taskbox/todo_item.js';

import {AutoTodoStatus, browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {AutoTodoItem, SourceReference} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {AiTaskboxElement} from 'chrome://context-hub/taskbox/ai_taskbox.js';
import type {TodoItemElement} from 'chrome://context-hub/taskbox/todo_item.js';
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
    handler.setResultFor('runTodoTask', Promise.resolve({success: true}));
    handler.setResultFor('updateAutoTodo', Promise.resolve({success: true}));
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  async function createItem(
      sourceReferences: SourceReference[], heading = 'Plan trip') {
    item = document.createElement('todo-item');
    item.heading = heading;
    item.sourceReferences = sourceReferences;
    document.body.appendChild(item);
    await microtasksFinished();
  }

  function query<T extends HTMLElement = HTMLElement>(selector: string): T|
      null {
    return item.shadowRoot.querySelector<T>(selector);
  }

  test('open tab opens web source references in tab group', async () => {
    await createItem([
      createHistoryRef('https://example.com/a', 'A'),
      createHistoryRef('chrome://settings', 'Settings'),
      createGmailRef('http://example.com/b', 'B'),
      createHistoryRef('file:///etc/passwd', 'File'),
    ]);

    const openTabButton = query<CrButtonElement>('#openTabButton')!;
    assertFalse(openTabButton.disabled);

    openTabButton.click();

    const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
    assertEquals('Plan trip', label);
    assertDeepEquals(['https://example.com/a', 'http://example.com/b'], urls);
    assertEquals(0, openWindowProxy.getCallCount('openUrl'));
  });

  test('open tab uses fallback group label when heading is empty', async () => {
    await createItem(
        [createHistoryRef('https://example.com/a')], /*heading=*/ '');

    query<CrButtonElement>('#openTabButton')!.click();

    const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
    assertEquals('Related Tabs', label);
    assertDeepEquals(['https://example.com/a'], urls);
  });

  test(
      'open tab falls back to opening individual tabs on failure', async () => {
        handler.setResultFor(
            'openUrlsInTabGroup', Promise.resolve({success: false}));
        const refs: SourceReference[] = [];
        for (let i = 0; i < 12; i++) {
          refs.push(createHistoryRef(`https://example.com/${i}`));
        }
        await createItem(refs);

        query<CrButtonElement>('#openTabButton')!.click();

        await handler.whenCalled('openUrlsInTabGroup');
        await microtasksFinished();

        assertEquals(10, openWindowProxy.getCallCount('openUrl'));
        assertDeepEquals(
            Array.from({length: 10}, (_, i) => `https://example.com/${i}`),
            openWindowProxy.getArgs('openUrl'));
      });

  test(
      'open tab is disabled when there are no actionableUrl or web references',
      async () => {
        await createItem([
          createHistoryRef('chrome://history'),
          createHistoryRef('file:///tmp/a'),
        ]);

        const openTabButton = query<CrButtonElement>('#openTabButton')!;
        assertTrue(openTabButton.disabled);
      });

  test(
      'split dropdown menu runs task with target url when can_act_on_web',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: true});
        await createItem(
            [createHistoryRef('https://example.com/a')], 'Book apartment tour');

        const dropdownButton = query<CrButtonElement>('#taskDropdownButton')!;
        dropdownButton.click();
        await microtasksFinished();

        const taskMenu = query<CrActionMenuElement>('#taskMenu')!;
        assertTrue(taskMenu.open);

        const menuItems =
            taskMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        assertEquals(2, menuItems.length);
        menuItems[1]!.click();

        const prompt = await handler.whenCalled('runTodoTask');
        await microtasksFinished();
        assertFalse(taskMenu.open);
        assertEquals(
            'Book apartment tour https://example.com/a If you need more ' +
                'information to complete your task or if you have any ' +
                'questions, ask.',
            prompt);
      });

  test(
      'split dropdown menu skips runTodoTask when heading and url are empty',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: true});
        await createItem([], /*heading=*/ '   ');

        query<CrButtonElement>('#taskDropdownButton')!.click();
        await microtasksFinished();

        const taskMenu = query<CrActionMenuElement>('#taskMenu')!;
        const menuItems =
            taskMenu.querySelectorAll<HTMLButtonElement>('.dropdown-item');
        menuItems[1]!.click();
        await microtasksFinished();

        assertFalse(taskMenu.open);
        assertEquals(0, handler.getCallCount('runTodoTask'));
      });

  test('dismiss button updates status to dismissed', async () => {
    await createItem([createHistoryRef('https://example.com/a')]);
    item.id = 'todo-1';
    await microtasksFinished();

    query<HTMLElement>('cr-expand-button')!.click();
    await microtasksFinished();

    query<HTMLElement>('.dismiss-button')!.click();
    const updatedTodo = await handler.whenCalled('updateAutoTodo');
    assertEquals('todo-1', updatedTodo.id);
    assertEquals(AutoTodoStatus.kDismissed, updatedTodo.status);
  });
});

suite('AiTaskbox', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let taskbox: AiTaskboxElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: false});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    const todos: AutoTodoItem[] = [
      {
        id: 'gmail-low',
        title: 'Low score Gmail todo',
        description: 'Desc 1',
        status: AutoTodoStatus.kActive,
        score: 0.4,
        data: {
          firstParty: {
            actionableUrl: 'https://example.com/1',
            sourceReferences: [createGmailRef('https://mail.google.com/1')],
          },
        },
      },
      {
        id: 'history-high',
        title: 'High score History todo',
        description: 'Desc 2',
        status: AutoTodoStatus.kActive,
        score: 0.9,
        data: {
          firstParty: {
            actionableUrl: null,
            sourceReferences: [createHistoryRef('https://example.com/2')],
          },
        },
      },
    ];

    handler.setResultFor('getAutoTodos', Promise.resolve({
      firstPartyTodos: todos,
      thirdPartyTodos: [],
      firstPartyMetadata:
          {lastGenerationTime: {internalValue: 0n}, hasError: false},
      thirdPartyMetadata:
          {lastGenerationTime: {internalValue: 0n}, hasError: false},
    }));
    handler.setResultFor('getTodoFeedbacks', Promise.resolve({feedbacks: []}));

    taskbox = document.createElement('ai-taskbox');
    document.body.appendChild(taskbox);
    await microtasksFinished();
  });

  test(
      'renders workspace and history todos in one list sorted by score', () => {
        const items = taskbox.shadowRoot.querySelectorAll<TodoItemElement>(
            '.todo-list todo-item');
        assertEquals(2, items.length);
        assertEquals('High score History todo', items[0]!.heading);
        assertEquals('Low score Gmail todo', items[1]!.heading);
      });
});
