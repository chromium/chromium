// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/overflow_menu.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {OverflowMenuElement} from 'chrome://contextual-tasks/overflow_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
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

    loadTimeData.resetForTesting({
      isSmallDeviceFormFactor: false,
      isSidePanelPinned: false,
      enablePinButton: false,
      isAiPage: false,
    });

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

  suite('SmallFormFactor', () => {
    setup(async () => {
      overflowMenu.isSmallDeviceFormFactor = true;
      await microtasksFinished();
    });

    test('shows correct items for Android', () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      assertEquals(3, buttons.length);
    });

    test('handles thread history click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const threadHistoryButton = buttons[0];
      assertTrue(!!threadHistoryButton);

      threadHistoryButton.click();
      await proxy.handler.whenCalled('showThreadHistory');
    });

    test('handles my activity click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const myActivityButton = buttons[1];
      assertTrue(!!myActivityButton);

      myActivityButton.click();
      await proxy.handler.whenCalled('openMyActivityUi');
    });

    test('handles help click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const helpButton = buttons[2];
      assertTrue(!!helpButton);

      helpButton.click();
      await proxy.handler.whenCalled('openFeedbackUi');
    });
  });

  suite('PinButton', () => {
    let metrics: MetricsTracker;

    setup(async () => {
      metrics = fakeMetricsPrivate();
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.resetForTesting({
        isSmallDeviceFormFactor: false,
        isSidePanelPinned: false,
        enablePinButton: true,
        isAiPage: true,
        isUserFeedbackAllowed: true,
        pinTooltip: 'Pin',
        unpinTooltip: 'Unpin',
      });
      overflowMenu = document.createElement('contextual-tasks-overflow-menu');
      document.body.appendChild(overflowMenu);
      await microtasksFinished();
    });

    test('records metrics on pin click', async () => {
      const pinButton =
          overflowMenu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);

      overflowMenu.isPinned = false;
      await microtasksFinished();

      pinButton.click();
      await proxy.handler.whenCalled('pinSidePanel');

      // Both recordUserAction and recordBoolean map to the same metric name in
      // the fake metrics tracker, resulting in a count of 2.
      assertEquals(
          2, metrics.count('ContextualTasks.WebUI.UserAction.PinSidePanel'));
      assertEquals(
          1,
          metrics.count('ContextualTasks.WebUI.UserAction.PinSidePanel', true));
      assertEquals(
          0, metrics.count('ContextualTasks.WebUI.UserAction.UnpinSidePanel'));
    });

    test('records metrics on unpin click', async () => {
      const pinButton =
          overflowMenu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);

      overflowMenu.isPinned = true;
      await microtasksFinished();

      pinButton.click();
      await proxy.handler.whenCalled('unpinSidePanel');

      // Both recordUserAction and recordBoolean map to the same metric name in
      // the fake metrics tracker, resulting in a count of 2.
      assertEquals(
          2, metrics.count('ContextualTasks.WebUI.UserAction.UnpinSidePanel'));
      assertEquals(
          1,
          metrics.count(
              'ContextualTasks.WebUI.UserAction.UnpinSidePanel', true));
      assertEquals(
          0, metrics.count('ContextualTasks.WebUI.UserAction.PinSidePanel'));
    });
  });
});
