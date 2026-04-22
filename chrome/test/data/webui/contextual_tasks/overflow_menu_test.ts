// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/overflow_menu.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {OverflowMenuElement} from 'chrome://contextual-tasks/overflow_menu.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('OverflowMenuTest', () => {
  let overflowMenu: OverflowMenuElement;
  let proxy: TestContextualTasksBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);

    overflowMenu = document.createElement('contextual-tasks-overflow-menu');
    document.body.appendChild(overflowMenu);
    await microtasksFinished();
  });

  test('handles open in new tab click', async () => {
    overflowMenu.enableOpenInNewTabButton = true;
    await microtasksFinished();

    const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
    const openInNewTabButton = buttons[0];
    assertTrue(!!openInNewTabButton);
    assertFalse(openInNewTabButton.disabled);

    openInNewTabButton.click();
    await proxy.handler.whenCalled('moveTaskUiToNewTab');

    overflowMenu.enableOpenInNewTabButton = false;
    await microtasksFinished();
    assertTrue(openInNewTabButton.disabled);
  });

  test('handles my activity click', async () => {
    const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
    const myActivityButton = buttons[1];
    assertTrue(!!myActivityButton);

    myActivityButton.click();
    await proxy.handler.whenCalled('openMyActivityUi');
  });

  test('handles feedback click', async () => {
    const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
    const feedbackButton = buttons[2];
    assertTrue(!!feedbackButton);

    feedbackButton.click();
    await proxy.handler.whenCalled('openFeedbackUi');
  });
});
