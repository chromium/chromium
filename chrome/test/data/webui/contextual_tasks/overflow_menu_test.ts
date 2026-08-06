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
      isUserFeedbackAllowed: true,
      contextualTasksEnableSpatialModelToolbarLayout: false,
      contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow: false,
      webuiRoundedIconsEnabled: false,
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

    test('handles feedback click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const feedbackButton = buttons[2];
      assertTrue(!!feedbackButton);

      feedbackButton.click();
      await proxy.handler.whenCalled('openFeedbackUi');
    });
  });

  suite('FeedbackDisabled', () => {
    setup(async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.resetForTesting({
        isSmallDeviceFormFactor: false,
        isSidePanelPinned: false,
        enablePinButton: false,
        isAiPage: false,
        isUserFeedbackAllowed: false,
        contextualTasksEnableSpatialModelToolbarLayout: false,
        contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow:
            false,
        webuiRoundedIconsEnabled: false,
      });
      overflowMenu = document.createElement('contextual-tasks-overflow-menu');
      document.body.appendChild(overflowMenu);
      await microtasksFinished();
    });

    test('hides feedback button', () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      assertEquals(2, buttons.length);
      const iconName = loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'contextual_tasks:feedback' :
          'contextual_tasks:feedback-old';
      const feedbackIcon = overflowMenu.shadowRoot.querySelector(
          `button cr-icon[icon="${iconName}"]`);
      assertFalse(!!feedbackIcon);
    });

    test('hides feedback button on small form factor', async () => {
      overflowMenu.isSmallDeviceFormFactor = true;
      await microtasksFinished();

      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      assertEquals(2, buttons.length);
      const iconName = loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'contextual_tasks:feedback' :
          'contextual_tasks:feedback-old';
      const feedbackIcon = overflowMenu.shadowRoot.querySelector(
          `button cr-icon[icon="${iconName}"]`);
      assertFalse(!!feedbackIcon);
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
        contextualTasksEnableSpatialModelToolbarLayout: false,
        contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow:
            false,
        webuiRoundedIconsEnabled: false,
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

  suite('ContextualTasksEnableSpatialModelToolbarLayout', () => {
    setup(async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.resetForTesting({
        isSmallDeviceFormFactor: false,
        isSidePanelPinned: false,
        enablePinButton: false,
        isAiPage: true,
        isUserFeedbackAllowed: true,
        contextualTasksEnableSpatialModelToolbarLayout: true,
        contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow:
            false,
        webuiRoundedIconsEnabled: false,
      });
      overflowMenu = document.createElement('contextual-tasks-overflow-menu');
      document.body.appendChild(overflowMenu);
      await microtasksFinished();
    });

    test('shows correct items in the menu', () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      // The menu should contain:
      // 1. Thread History (because we are on AI page and the flag is enabled)
      // 2. My Activity
      // 3. Help button
      // 4. Feedback button
      // No open in new tab (hidden by flag).
      assertEquals(4, buttons.length);

      const threadHistoryButton = buttons[0];
      assertTrue(!!threadHistoryButton);
      const historyIcon = threadHistoryButton.querySelector('cr-icon');
      assertTrue(!!historyIcon);
      assertEquals(
          loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
              'contextual_tasks:notes-spark' :
              'contextual_tasks:notes_spark-old',
          historyIcon.getAttribute('icon'));
    });

    test('handles thread history click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const threadHistoryButton = buttons[0];
      assertTrue(!!threadHistoryButton);

      threadHistoryButton.click();
      await proxy.handler.whenCalled('showThreadHistory');
    });

    test('handles help click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const helpButton = buttons[2];
      assertTrue(!!helpButton);

      helpButton.click();
      await proxy.handler.whenCalled('openOverflowMenuHelpUi');
    });

    test('handles feedback click', async () => {
      const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
      const feedbackButton = buttons[3];
      assertTrue(!!feedbackButton);

      feedbackButton.click();
      await proxy.handler.whenCalled('openFeedbackUi');
    });

    suite('WithNewThreadInOverflow', () => {
      setup(async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.resetForTesting({
          isSmallDeviceFormFactor: false,
          isSidePanelPinned: false,
          enablePinButton: false,
          isAiPage: true,
          isUserFeedbackAllowed: true,
          contextualTasksEnableSpatialModelToolbarLayout: true,
          contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow:
              true,
          webuiRoundedIconsEnabled: false,
        });
        overflowMenu = document.createElement('contextual-tasks-overflow-menu');
        overflowMenu.isAimEligible = true;
        document.body.appendChild(overflowMenu);
        await microtasksFinished();
      });

      test(
          'shows new thread inside the menu and fires click event',
          async () => {
            const buttons = overflowMenu.shadowRoot.querySelectorAll('button');
            // The menu should contain:
            // 1. New Thread
            // 2. Thread History
            // 3. My Activity
            // 4. Help button
            // 5. Feedback button
            assertEquals(5, buttons.length);

            const newThreadButton = buttons[0];
            assertTrue(!!newThreadButton);
            assertEquals('newThreadButton', newThreadButton.id);

            const newThreadIcon = newThreadButton.querySelector('cr-icon');
            assertTrue(!!newThreadIcon);
            assertEquals(
                loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
                    'contextual_tasks:edit-square' :
                    'contextual_tasks:edit_square-old',
                newThreadIcon.getAttribute('icon'));

            let clickEventFired = false;
            overflowMenu.addEventListener('new-thread-click', () => {
              clickEventFired = true;
            });

            newThreadButton.click();
            await microtasksFinished();

            assertTrue(
                clickEventFired, 'new-thread-click event should be emitted');
          });
    });
  });

  suite('SpatialModelOpenInNewTabSuppression', () => {
    function getOpenInNewTabButton() {
      const iconName = loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'contextual_tasks:open-in-full' :
          'contextual_tasks:open_in_full_tab-old';
      return overflowMenu.shadowRoot.querySelector(
          `button cr-icon[icon="${iconName}"]`);
    }

    test('shown when both spatial model flags are false', async () => {
      overflowMenu.contextualTasksEnableSpatialModelToolbarLayout = false;
      overflowMenu.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow = false;
      await microtasksFinished();

      assertTrue(!!getOpenInNewTabButton());
    });

    test('hidden when contextualTasksEnableSpatialModelToolbarLayout is true', async () => {
      overflowMenu.contextualTasksEnableSpatialModelToolbarLayout = true;
      overflowMenu.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow = false;
      await microtasksFinished();

      assertFalse(!!getOpenInNewTabButton());
    });

    test('hidden when contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow is true', async () => {
      overflowMenu.contextualTasksEnableSpatialModelToolbarLayout = false;
      overflowMenu.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow = true;
      await microtasksFinished();

      assertFalse(!!getOpenInNewTabButton());
    });
  });
});
