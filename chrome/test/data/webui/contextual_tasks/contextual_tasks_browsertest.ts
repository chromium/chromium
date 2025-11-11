// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

const fixtureUrl = 'chrome://webui-test/contextual_tasks/test.html';

suite('ContextualTasksAppTest', function() {
  test('gets thread url', async () => {
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    assertEquals(await proxy.handler.getCallCount('getThreadUrl'), 1);
  });

  test('gets task url when query param set', async () => {
    // Set a task Uuid as a query parameter.
    const taskId = '123';
    window.history.replaceState({}, '', `?task=${taskId}`);

    // Set the q query parameter for the AI page.
    const query = 'abc';
    const fixtureUrlWithQuery = `${fixtureUrl}?q=${query}`;
    const proxy = new TestContextualTasksBrowserProxy(fixtureUrlWithQuery);
    BrowserProxyImpl.setInstance(proxy);

    document.body.appendChild(document.createElement('contextual-tasks-app'));

    assertDeepEquals(
        await proxy.handler.whenCalled('getUrlForTask'), {value: taskId});
    assertDeepEquals(
        await proxy.handler.whenCalled('setTaskId'), {value: taskId});
    assertEquals(await proxy.handler.whenCalled('setThreadTitle'), query);
  });
});
