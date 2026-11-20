// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/taskbox/ai_taskbox.js';
import 'chrome://context-hub/taskbox/todo_item.js';

import {AutoTodoStatus, browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {AutoTodoItem, PageRemote, SourceReference} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {AiTaskboxElement} from 'chrome://context-hub/taskbox/ai_taskbox.js';
import type {TodoItemElement} from 'chrome://context-hub/taskbox/todo_item.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  test('feedback mode toggles thumbs and score visibility', async () => {
    await createItem([createGmailRef('https://mail.google.com/1')]);
    item.score = 0.85;
    await microtasksFinished();

    assertNull(query('.thumb-group'));
    assertNull(query('.score-badge'));

    item.feedbackMode = true;
    await microtasksFinished();

    assertTrue(!!query('.thumb-group'));
    assertTrue(!!query('#thumbsUp'));
    assertTrue(!!query('#thumbsDown'));
    assertEquals('Score: 0.85', query('.score-badge')!.textContent.trim());

    // Undefined score should not render a score badge or throw during render.
    item.score = undefined;
    await microtasksFinished();
    assertNull(query('.score-badge'));

    // Score of 0 should still render.
    item.score = 0;
    await microtasksFinished();
    assertEquals('Score: 0.00', query('.score-badge')!.textContent.trim());
  });

  test('check circle reflects active and completed status', async () => {
    await createItem([createHistoryRef('https://example.com/a')]);

    const checkCircle = query<CrIconButtonElement>('#check-circle')!;
    assertEquals(
        'context-hub:radio-button-unchecked',
        checkCircle.getAttribute('iron-icon'));
    assertFalse(checkCircle.classList.contains('completed'));

    item.status = AutoTodoStatus.kCompleted;
    await microtasksFinished();

    assertEquals('cr:check-circle', checkCircle.getAttribute('iron-icon'));
    assertTrue(checkCircle.classList.contains('completed'));
  });

  test('renders inline reference links without expansion', async () => {
    await createItem([]);
    assertNull(query('.todo-metadata'));

    await createItem([
      createHistoryRef('chrome://settings', 'Settings'),
      createHistoryRef('file:///etc/passwd', 'File'),
      createGmailRef('', 'Empty Gmail'),
      createHistoryRef('javascript:alert(1)', 'Script'),
    ]);
    assertNull(query('.todo-metadata'));

    await createItem([
      createGmailRef('https://mail.google.com/1', 'Flight itinerary'),
      createHistoryRef('https://example.com/hotel', '  '),
      createHistoryRef('chrome://history', 'History'),
    ]);

    const links = item.shadowRoot.querySelectorAll<HTMLAnchorElement>(
        '.todo-metadata .reference-link');
    assertEquals(2, links.length);
    assertEquals('https://mail.google.com/1', links[0]!.getAttribute('href'));
    assertEquals('Flight itinerary', links[0]!.textContent.trim());
    assertEquals('https://example.com/hotel', links[1]!.getAttribute('href'));
    assertEquals('https://example.com/hotel', links[1]!.textContent.trim());
  });

  test('dismiss menu item updates status to dismissed', async () => {
    await createItem([createHistoryRef('https://example.com/a')]);
    item.id = 'todo-1';
    await microtasksFinished();

    const moreButton = query<CrIconButtonElement>('#moreButton')!;
    assertEquals('menu', moreButton.getAttribute('aria-haspopup'));
    assertFalse(moreButton.disabled);

    moreButton.click();
    await microtasksFinished();

    const menu = query<CrActionMenuElement>('#menu')!;
    assertTrue(menu.open);

    query<HTMLButtonElement>('#dismissButton')!.click();
    const updatedTodo = await handler.whenCalled('updateAutoTodo');
    await microtasksFinished();
    assertFalse(menu.open);
    assertEquals('todo-1', updatedTodo.id);
    assertEquals(AutoTodoStatus.kDismissed, updatedTodo.status);
  });

  test(
      'dismissed todo restores active status via menu without duplicate button',
      async () => {
        await createItem([createHistoryRef('https://example.com/a')]);
        item.id = 'todo-1';
        item.status = AutoTodoStatus.kDismissed;
        await microtasksFinished();

        assertNull(query('#markActiveButton'));
        assertNull(query('#check-circle'));

        query<CrIconButtonElement>('#moreButton')!.click();
        await microtasksFinished();

        const menu = query<CrActionMenuElement>('#menu')!;
        assertTrue(menu.open);
        assertNull(query('#dismissButton'));

        query<HTMLButtonElement>('#menuMarkActiveButton')!.click();
        const updatedTodo = await handler.whenCalled('updateAutoTodo');
        await microtasksFinished();
        assertFalse(menu.open);
        assertEquals('todo-1', updatedTodo.id);
        assertEquals(AutoTodoStatus.kActive, updatedTodo.status);
      });

  test(
      'run task button renders on left of open tab and invokes runTodoTask',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: true});
        await createItem(
            [createHistoryRef('https://example.com/a')], 'Book apartment tour');

        const actions = query('.todo-actions')!;
        const buttons = actions.querySelectorAll('cr-button');
        assertEquals(2, buttons.length);
        assertEquals('runTaskButton', buttons[0]!.id);
        assertEquals('openTabButton', buttons[1]!.id);
        assertEquals('Open tab', buttons[1]!.textContent.trim());
        assertNull(buttons[1]!.querySelector('cr-icon'));

        const icon = buttons[0]!.querySelector('cr-icon')!;
        assertEquals('context-hub:cursor-spark', icon.getAttribute('icon'));

        buttons[0]!.click();
        const prompt = await handler.whenCalled('runTodoTask');
        assertEquals(
            'Book apartment tour https://example.com/a If you need more ' +
                'information to complete your task or if you have any ' +
                'questions, ask.',
            prompt);
      });

  test(
      'run task button disables while in flight and during state management',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: true});
        const resolver = new PromiseResolver<{success: boolean}>();
        handler.setResultFor('runTodoTask', resolver.promise);
        await createItem(
            [createHistoryRef('https://example.com/a')], 'Book tour');

        const runTaskButton = query<CrButtonElement>('#runTaskButton')!;
        assertFalse(runTaskButton.disabled);

        runTaskButton.click();
        await microtasksFinished();
        assertTrue(runTaskButton.disabled);

        // Clicking again while in flight should not trigger a duplicate call.
        runTaskButton.click();
        await microtasksFinished();
        assertEquals(1, handler.getCallCount('runTodoTask'));

        resolver.resolve({success: true});
        await microtasksFinished();
        assertFalse(runTaskButton.disabled);

        item.disable_state_mgmt = true;
        await microtasksFinished();
        assertTrue(runTaskButton.disabled);
        assertTrue(query<CrIconButtonElement>('#moreButton')!.disabled);
      });

  test(
      'run task button is disabled and skips runTodoTask when heading and ' +
          'url are empty',
      async () => {
        loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: true});
        await createItem([], /*heading=*/ '   ');

        const runTaskButton = query<CrButtonElement>('#runTaskButton')!;
        assertTrue(runTaskButton.disabled);

        runTaskButton.click();
        await microtasksFinished();

        assertEquals(0, handler.getCallCount('runTodoTask'));
      });
  test('hides run task button when kAutoTodos is disabled', async () => {
    loadTimeData.overrideValues({kAutoTodos: false, can_act_on_web: true});
    await createItem(
        [createHistoryRef('https://example.com/a')], 'Book apartment tour');

    assertNull(query('#runTaskButton'));
  });

  test('disable_feedback disables thumb buttons', async () => {
    item = document.createElement('todo-item');
    item.id = 'task-1';
    item.heading = 'Review doc';
    item.feedbackMode = true;
    let feedbackChangedCount = 0;
    item.addEventListener('feedback-changed', () => {
      feedbackChangedCount++;
    });
    document.body.appendChild(item);
    await microtasksFinished();

    const thumbsUp = query<CrIconButtonElement>('#thumbsUp')!;
    const thumbsDown = query<CrIconButtonElement>('#thumbsDown')!;

    // Setting `disable_feedback` guards `onThumbClick_()` immediately, even
    // before Lit flushes `disabled` onto `<cr-icon-button>`.
    item.disable_feedback = true;
    thumbsUp.click();
    thumbsDown.click();
    await microtasksFinished();

    assertTrue(thumbsUp.disabled);
    assertTrue(thumbsDown.disabled);
    assertEquals('none', getComputedStyle(thumbsUp).pointerEvents);
    assertEquals('none', getComputedStyle(thumbsDown).pointerEvents);

    thumbsUp.click();
    thumbsDown.click();
    await microtasksFinished();

    assertEquals(0, handler.getCallCount('setTodoFeedback'));
    assertEquals(0, handler.getCallCount('deleteTodoFeedback'));
    assertEquals(0, feedbackChangedCount);
  });
});

suite('AiTaskbox', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let callbackRemote: PageRemote;
  let taskbox: AiTaskboxElement;
  let todos: AutoTodoItem[];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kAutoTodos: true, can_act_on_web: false});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    callbackRemote = remote;
    browserProxyFactory.setInstance(instance);

    todos = [
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

  test('renders header count with plural and singular to-do text', async () => {
    const header = taskbox.shadowRoot.querySelector('h1')!;
    const normalize = (s: string) => s.replace(/\s+/g, ' ').trim();
    assertEquals(
        'You have 2 to-dos to catch up on', normalize(header.textContent));

    callbackRemote.onAutoTodosChanged([todos[0]!]);
    await callbackRemote.$.flushForTesting();
    await microtasksFinished();
    assertEquals(
        'You have 1 to-do to catch up on', normalize(header.textContent));
  });

  test('feedback mode is enabled by default and toggleable', async () => {
    const toggle = taskbox.shadowRoot.querySelector<CrToggleElement>(
        '#feedbackModeToggle')!;
    assertTrue(toggle.checked);
    assertTrue(!!taskbox.shadowRoot.querySelector('#generalFeedbackButton'));

    const firstItem =
        taskbox.shadowRoot.querySelector<TodoItemElement>('todo-item')!;
    assertTrue(firstItem.feedbackMode);
    assertTrue(!!firstItem.shadowRoot.querySelector('.thumb-group'));

    toggle.click();
    await microtasksFinished();

    assertFalse(toggle.checked);
    assertNull(taskbox.shadowRoot.querySelector('#generalFeedbackButton'));
    assertFalse(firstItem.feedbackMode);
    assertNull(firstItem.shadowRoot.querySelector('.thumb-group'));

    toggle.click();
    await microtasksFinished();

    assertTrue(toggle.checked);
    assertTrue(!!taskbox.shadowRoot.querySelector('#generalFeedbackButton'));
    assertTrue(firstItem.feedbackMode);
    assertTrue(!!firstItem.shadowRoot.querySelector('.thumb-group'));
  });
});
