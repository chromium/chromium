// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/taskbox/ai_taskbox.js';

import {AutoTodoStatus, browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {LaunchpadTask, SourceReference} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {AiTaskboxElement} from 'chrome://context-hub/taskbox/ai_taskbox.js';
import type {TodoItemElement} from 'chrome://context-hub/taskbox/todo_item.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createGmailRef(messageUrl: string, subject = 'Mail'): SourceReference {
  return {
    gmail: {
      messageUrl,
      subject,
    },
  };
}

function createHistoryRef(url: string, title = 'Page'): SourceReference {
  return {
    chromeHistoryEntry: {
      url,
      title,
      visitTimestamp: {internalValue: 0n},
    },
  };
}

function createLaunchpadTask(overrides: Partial<LaunchpadTask>): LaunchpadTask {
  return {
    id: 'task-1',
    title: 'Default Task',
    description: 'Description',
    status: AutoTodoStatus.kActive,
    score: 0.5,
    sourceReferences: [],
    actionableUrl: 'https://example.com/action',
    ...overrides,
  };
}

suite('AiTaskboxLaunchpad', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let taskbox: AiTaskboxElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      kLaunchpad: true,
      kAutoTodos: false,
      can_act_on_web: false,
    });
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor('getTasks', Promise.resolve({tasks: []}));
  });

  async function createTaskbox() {
    taskbox = document.createElement('ai-taskbox');
    document.body.appendChild(taskbox);
    await microtasksFinished();
  }

  function $<T extends HTMLElement = HTMLElement>(selector: string): T|null {
    return taskbox.shadowRoot.querySelector<T>(selector);
  }

  function activeItems(): TodoItemElement[] {
    const activeList =
        taskbox.shadowRoot.querySelector('.todo-column > .todo-list')!;
    return Array.from(
        activeList.querySelectorAll<TodoItemElement>('todo-item'));
  }

  test('loads cached Launchpad tasks sorted by score', async () => {
    handler.setResultFor('getTasks', Promise.resolve({
      tasks: [
        createLaunchpadTask({
          id: 'ws-low',
          title: 'Low Priority Mail',
          score: 0.4,
          sourceReferences: [createGmailRef('https://mail.google.com/1')],
        }),
        createLaunchpadTask({
          id: 'ws-high',
          title: 'High Priority Mail',
          score: 0.9,
          sourceReferences: [createGmailRef('https://mail.google.com/2')],
        }),
        createLaunchpadTask({
          id: 'ws-completed',
          title: 'Done Mail',
          status: AutoTodoStatus.kCompleted,
          score: 0.8,
        }),
        createLaunchpadTask({
          id: 'hist-active',
          title: 'Resume Trip Research',
          score: 0.7,
          actionableUrl: null,
          sourceReferences: [createHistoryRef('https://example.com/trip')],
        }),
      ],
    }));

    await createTaskbox();

    assertEquals(1, handler.getCallCount('getTasks'));
    assertEquals(0, handler.getCallCount('getAutoTodos'));
    assertEquals(0, handler.getCallCount('getTodoFeedbacks'));

    // Clear dropdown and menu are hidden when Launchpad is enabled.
    assertNull($('#workspaceDropdownButton'));
    assertNull($('#workspaceMenu'));

    const refreshButton = $<CrIconButtonElement>('#refreshButton')!;
    assertFalse(refreshButton.disabled);

    // Active Workspace and History tasks render in one list sorted by score.
    const items = activeItems();
    assertDeepEquals(
        ['High Priority Mail', 'Resume Trip Research', 'Low Priority Mail'],
        items.map(el => el.heading));
    for (const item of items) {
      assertTrue(item.disable_state_mgmt);
      assertTrue(item.disable_feedback);
    }

    // Completed section count reflects the completed task.
    const completedHeading = $('.completed-expand-button h2')!;
    assertEquals('Completed To-dos (1)', completedHeading.textContent.trim());
  });

  test('clicking refresh calls generateTasks and updates list', async () => {
    await createTaskbox();

    const refreshButton = $<CrIconButtonElement>('#refreshButton')!;
    const statusText = $('.last-updated-text')!;
    assertFalse(refreshButton.disabled);
    assertEquals(
        'Click the refresh icon to generate', statusText.textContent.trim());
    assertEquals('No To-dos yet.', $('.placeholder-text')!.textContent.trim());

    const resolver = new PromiseResolver<{tasks: LaunchpadTask[] | null}>();
    handler.setResultFor('generateTasks', resolver.promise);

    refreshButton.click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('generateTasks'));
    assertTrue(refreshButton.disabled);
    assertEquals('Generating...', statusText.textContent.trim());

    resolver.resolve({
      tasks: [
        createLaunchpadTask({
          id: 'generated-1',
          title: 'Generated Task',
          score: 0.95,
        }),
      ],
    });
    await microtasksFinished();

    assertFalse(refreshButton.disabled);
    assertEquals('Last updated just now', statusText.textContent.trim());
    assertDeepEquals(['Generated Task'], activeItems().map(el => el.heading));
  });

  test('clicking refresh shows error state on null tasks', async () => {
    await createTaskbox();
    handler.setResultFor('generateTasks', Promise.resolve({tasks: null}));

    $<CrIconButtonElement>('#refreshButton')!.click();
    await microtasksFinished();

    assertFalse($<CrIconButtonElement>('#refreshButton')!.disabled);
    assertEquals(
        'Failed to generate', $('.last-updated-text')!.textContent.trim());
    assertEquals(
        'Failed to generate. Please try again.',
        $('.placeholder-text')!.textContent.trim());
  });

  test(
      'ignores stale getTasks when refresh starts before load resolves',
      async () => {
        const getTasksResolver =
            new PromiseResolver<{tasks: LaunchpadTask[]}>();
        handler.setResultFor('getTasks', getTasksResolver.promise);
        handler.setResultFor('generateTasks', Promise.resolve({
          tasks: [
            createLaunchpadTask({
              id: 'fresh-1',
              title: 'Freshly Generated Task',
              score: 0.9,
            }),
          ],
        }));

        await createTaskbox();
        assertEquals(1, handler.getCallCount('getTasks'));

        // User triggers refresh before initial `getTasks()` resolves.
        $<CrIconButtonElement>('#refreshButton')!.click();
        await microtasksFinished();
        assertDeepEquals(
            ['Freshly Generated Task'], activeItems().map(el => el.heading));

        // Late resolution of initial `getTasks()` must not clobber fresh tasks.
        getTasksResolver.resolve({
          tasks: [
            createLaunchpadTask({
              id: 'stale-1',
              title: 'Stale Cached Task',
              score: 0.5,
            }),
          ],
        });
        await microtasksFinished();

        assertDeepEquals(
            ['Freshly Generated Task'], activeItems().map(el => el.heading));
      });

  test(
      'cached load with only completed tasks keeps placeholder consistent ' +
          'with unset generation time',
      async () => {
        handler.setResultFor('getTasks', Promise.resolve({
          tasks: [
            createLaunchpadTask({
              id: 'done-1',
              title: 'Completed Cached Task',
              status: AutoTodoStatus.kCompleted,
              score: 0.8,
            }),
          ],
        }));

        await createTaskbox();

        assertEquals(0, activeItems().length);
        assertEquals(
            'Completed To-dos (1)',
            $('.completed-expand-button h2')!.textContent.trim());
        assertEquals(
            'Click the refresh icon to generate',
            $('.last-updated-text')!.textContent.trim());
        assertEquals(
            'No To-dos yet.', $('.placeholder-text')!.textContent.trim());

        // After an explicit generation returning no active tasks, both the
        // timestamp and "You're all caught up!" state update together.
        handler.setResultFor('generateTasks', Promise.resolve({tasks: []}));
        $<CrIconButtonElement>('#refreshButton')!.click();
        await microtasksFinished();

        assertEquals(
            'Last updated just now',
            $('.last-updated-text')!.textContent.trim());
        assertEquals(
            'You\'re all caught up!',
            $('.placeholder-text')!.textContent.trim());
      });

  test('ignores stale getTasks across disconnect and reconnect', async () => {
    const firstLoadResolver = new PromiseResolver<{tasks: LaunchpadTask[]}>();
    handler.setResultFor('getTasks', firstLoadResolver.promise);

    await createTaskbox();
    assertEquals(1, handler.getCallCount('getTasks'));

    // Disconnect while the first `getTasks()` call is still in flight.
    taskbox.remove();

    const secondLoadResolver = new PromiseResolver<{tasks: LaunchpadTask[]}>();
    handler.setResultFor('getTasks', secondLoadResolver.promise);
    document.body.appendChild(taskbox);
    await microtasksFinished();
    assertEquals(2, handler.getCallCount('getTasks'));

    secondLoadResolver.resolve({
      tasks: [
        createLaunchpadTask({
          id: 'reconnected-1',
          title: 'Reconnected Task',
          score: 0.9,
        }),
      ],
    });
    await microtasksFinished();
    assertDeepEquals(['Reconnected Task'], activeItems().map(el => el.heading));

    // Late resolution from the prior connection must not overwrite tasks.
    firstLoadResolver.resolve({
      tasks: [
        createLaunchpadTask({
          id: 'stale-disconnected-1',
          title: 'Stale Disconnected Task',
          score: 0.5,
        }),
      ],
    });
    await microtasksFinished();
    assertDeepEquals(['Reconnected Task'], activeItems().map(el => el.heading));
  });

  test(
      'ignores stale generateTasks across disconnect and reconnect',
      async () => {
        await createTaskbox();

        const firstGenerateResolver =
            new PromiseResolver<{tasks: LaunchpadTask[] | null}>();
        handler.setResultFor('generateTasks', firstGenerateResolver.promise);

        const refreshButton = $<CrIconButtonElement>('#refreshButton')!;
        refreshButton.click();
        await microtasksFinished();
        assertEquals(1, handler.getCallCount('generateTasks'));
        assertTrue(refreshButton.disabled);

        // Disconnect while generation is in flight, then reconnect.
        taskbox.remove();
        document.body.appendChild(taskbox);
        await microtasksFinished();
        assertFalse(refreshButton.disabled);

        // Start a new generation after reconnecting.
        const secondGenerateResolver =
            new PromiseResolver<{tasks: LaunchpadTask[] | null}>();
        handler.setResultFor('generateTasks', secondGenerateResolver.promise);
        refreshButton.click();
        await microtasksFinished();
        assertEquals(2, handler.getCallCount('generateTasks'));
        assertTrue(refreshButton.disabled);
        assertEquals(
            'Generating...', $('.last-updated-text')!.textContent.trim());

        // Stale failure from the disconnected generation must not clear the
        // new in-flight generation or surface an error.
        firstGenerateResolver.resolve({tasks: null});
        await microtasksFinished();
        assertTrue(refreshButton.disabled);
        assertEquals(
            'Generating...', $('.last-updated-text')!.textContent.trim());

        secondGenerateResolver.resolve({
          tasks: [
            createLaunchpadTask({
              id: 'fresh-reconnected-1',
              title: 'Fresh Reconnected Task',
              score: 0.95,
            }),
          ],
        });
        await microtasksFinished();
        assertFalse(refreshButton.disabled);
        assertEquals(
            'Last updated just now',
            $('.last-updated-text')!.textContent.trim());
        assertDeepEquals(
            ['Fresh Reconnected Task'], activeItems().map(el => el.heading));
      });

  test(
      'prefers Launchpad over ContextHubService when both flags are enabled',
      async () => {
        loadTimeData.overrideValues({
          kLaunchpad: true,
          kAutoTodos: true,
          can_act_on_web: false,
        });

        handler.setResultFor('getTasks', Promise.resolve({
          tasks:
              [createLaunchpadTask({id: 'lp-1', title: 'Launchpad 1P Task'})],
        }));

        await createTaskbox();

        assertEquals(1, handler.getCallCount('getTasks'));
        assertEquals(0, handler.getCallCount('getAutoTodos'));
        assertDeepEquals(
            ['Launchpad 1P Task'], activeItems().map(el => el.heading));
      });
});
