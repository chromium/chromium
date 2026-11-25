// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

suite('PerformanceInterventionButton', function() {
  let button: any;
  let browserProxy: TestToolbarBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    button = document.createElement('performance-intervention-button');
    document.body.appendChild(button);
    await button.updateComplete;
    await microtasksFinished();
  });

  test('ClickTriggersProxy', async () => {
    assertEquals(
        0,
        browserProxy.toolbarUIHandler.getCallCount(
            'onPerformanceInterventionButtonClicked'));

    assertTrue(!!button.shadowRoot, 'shadowRoot should not be null');
    const crIconButton = button.shadowRoot?.querySelector('cr-icon-button');
    assertTrue(!!crIconButton, 'cr-icon-button should not be null');

    // Simulate click
    crIconButton.click();

    await browserProxy.toolbarUIHandler.whenCalled(
        'onPerformanceInterventionButtonClicked');
    assertEquals(
        1,
        browserProxy.toolbarUIHandler.getCallCount(
            'onPerformanceInterventionButtonClicked'));
  });

  test('ShowsTooltip', () => {
    const crIconButton = button.shadowRoot?.querySelector('cr-icon-button');
    assertTrue(!!crIconButton);
    // Tooltip and aria-label strings should be set.
    assertEquals('Performance issue alert', crIconButton.title);
    assertEquals(
        'Performance issue alert', crIconButton.getAttribute('aria-label'));
  });

  test('ClickSuppression', async () => {
    const crIconButton = button.shadowRoot!.querySelector('cr-icon-button')!;
    const handler = browserProxy.toolbarUIHandler;

    const dispatchMouseClick = () => {
      crIconButton.dispatchEvent(new PointerEvent(
          'pointerdown', {bubbles: true, button: 0, pointerType: 'mouse'}));
      crIconButton.dispatchEvent(new PointerEvent(
          'click', {bubbles: true, button: 0, pointerType: 'mouse'}));
    };

    // 1. Normal click when not highlighted triggers proxy.
    dispatchMouseClick();
    assertEquals(
        1, handler.getCallCount('onPerformanceInterventionButtonClicked'));

    // 2. Click while highlighted is suppressed.
    button.highlightTracker.onHighlightChanged(true);
    dispatchMouseClick();
    assertEquals(
        1, handler.getCallCount('onPerformanceInterventionButtonClicked'));

    // 3. Click right after unhighlighting (< 100ms) is suppressed.
    button.highlightTracker.onHighlightChanged(false);
    dispatchMouseClick();
    assertEquals(
        1, handler.getCallCount('onPerformanceInterventionButtonClicked'));

    // 4. Keyboard activation is not suppressed even after a pointerdown
    // that armed click skipping.
    crIconButton.dispatchEvent(new PointerEvent(
        'pointerdown', {bubbles: true, button: 0, pointerType: 'mouse'}));
    crIconButton.dispatchEvent(
        new PointerEvent('click', {bubbles: true, button: 0, pointerType: ''}));
    assertEquals(
        2, handler.getCallCount('onPerformanceInterventionButtonClicked'));

    // 5. Click >= 100ms after unhighlighting triggers proxy.
    await new Promise(resolve => setTimeout(resolve, 150));
    dispatchMouseClick();
    assertEquals(
        3, handler.getCallCount('onPerformanceInterventionButtonClicked'));
  });

  test('ActiveState', async () => {
    assertFalse(button.hasAttribute('is-active'));

    // 1. When shown, is-active is present.
    button.state = {shouldBeShown: true};
    await button.updateComplete;
    assertTrue(button.hasAttribute('is-active'));

    // 2. Highlighting and unhighlighting without clicking the button keeps
    // is-active present.
    button.highlightTracker.onHighlightChanged(true);
    await button.updateComplete;
    assertTrue(button.hasAttribute('is-active'));

    button.highlightTracker.onHighlightChanged(false);
    await button.updateComplete;
    assertTrue(button.hasAttribute('is-active'));

    // 3. Clicking the button deactivates it.
    const crIconButton = button.shadowRoot!.querySelector('cr-icon-button')!;
    crIconButton.click();
    await button.updateComplete;
    assertFalse(button.hasAttribute('is-active'));

    // 4. Unrelated state updates while shouldBeShown remains true do not
    // re-activate the button.
    button.state = {shouldBeShown: true};
    await button.updateComplete;
    assertFalse(button.hasAttribute('is-active'));

    // 5. Hiding and re-showing resets is-active to true.
    button.state = {shouldBeShown: false};
    await button.updateComplete;
    button.state = {shouldBeShown: true};
    await button.updateComplete;
    assertTrue(button.hasAttribute('is-active'));
  });
});
