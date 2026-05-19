// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WindowManager} from 'chrome://contextual-tasks/window_manager.js';
import type {WebViewType} from 'chrome://contextual-tasks/web_view_type.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

interface MockNewWindowEvent {
  preventDefault(): void;
  window: {
    attach(webview: HTMLElement): void,
  };
}

interface MockWebview {
  addEventListener(type: 'newwindow', callback: (e: MockNewWindowEvent) => void): void;
}

interface WebViewWithPartition extends HTMLElement {
  partition: string;
}

suite('WindowManagerTest', () => {
  let windowManager: WindowManager;
  let mockMainWebview: MockWebview;
  let newWindowCallback: ((e: MockNewWindowEvent) => void)|null = null;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockMainWebview = {
      addEventListener: (type: 'newwindow', callback: (e: MockNewWindowEvent) => void) => {
        if (type === 'newwindow') {
          newWindowCallback = callback;
        }
      },
    };

    windowManager = new WindowManager(mockMainWebview as unknown as WebViewType);
    assertTrue(!!windowManager);
    await microtasksFinished();
  });

  test('creates mock webview on newwindow event', () => {
    assertTrue(newWindowCallback !== null);

    let preventDefaultCalled = false;
    let attachCalled = false;
    let attachedWebview: WebViewWithPartition|null = null;

    const mockEvent: MockNewWindowEvent = {
      preventDefault: () => {
        preventDefaultCalled = true;
      },
      window: {
        attach: (webview: HTMLElement) => {
          attachCalled = true;
          attachedWebview = webview as WebViewWithPartition;
        },
      },
    };

    newWindowCallback(mockEvent);

    assertTrue(preventDefaultCalled);
    assertTrue(attachCalled);
    assertTrue(attachedWebview !== null);

    // Verify it was added to DOM
    const webviews = document.querySelectorAll('webview');
    assertEquals(1, webviews.length);
    assertEquals(attachedWebview, webviews[0]);
    const attachedWebviewNonNull = attachedWebview as unknown as WebViewWithPartition;
    assertEquals('persist:contextual-tasks', attachedWebviewNonNull.partition);
    assertEquals('none', attachedWebviewNonNull.style.display);
  });

  test('removes webview on close event', async () => {
    assertTrue(newWindowCallback !== null);

    const mockEvent: MockNewWindowEvent = {
      preventDefault: () => {},
      window: {
        attach: (_webview: HTMLElement) => {},
      },
    };

    newWindowCallback(mockEvent);

    const webviews = document.querySelectorAll('webview');
    assertEquals(1, webviews.length);
    const attachedWebview = webviews[0]!;

    // Simulate close event
    attachedWebview.dispatchEvent(new Event('close'));
    await microtasksFinished();

    const webviewsAfterClose = document.querySelectorAll('webview');
    assertEquals(0, webviewsAfterClose.length);
  });
});
