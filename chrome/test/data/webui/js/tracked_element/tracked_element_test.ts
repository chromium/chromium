// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
import type {TrackedElementProxy} from 'chrome://resources/js/tracked_element/tracked_element_proxy.js';
import {TrackedElementProxyImpl} from 'chrome://resources/js/tracked_element/tracked_element_proxy.js';
import type {RectF} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {TrackedElementHandlerInterface} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class MockTrackedElementHandler extends TestBrowserProxy implements
    TrackedElementHandlerInterface {
  constructor() {
    super([
      'trackedElementVisibilityChanged',
      'trackedElementActivated',
      'trackedElementCustomEvent',
    ]);
  }

  trackedElementVisibilityChanged(
      nativeIdentifier: string, visible: boolean, rect: RectF) {
    this.methodCalled(
        'trackedElementVisibilityChanged', nativeIdentifier, visible, rect);
  }

  trackedElementActivated(nativeIdentifier: string) {
    this.methodCalled('trackedElementActivated', nativeIdentifier);
  }

  trackedElementCustomEvent(nativeIdentifier: string, eventName: string) {
    this.methodCalled('trackedElementCustomEvent', nativeIdentifier, eventName);
  }
}

class TestTrackedElementProxy extends TestBrowserProxy implements
    TrackedElementProxy {
  handler = new MockTrackedElementHandler();

  constructor() {
    super(['getHandler']);
  }

  getHandler() {
    this.methodCalled('getHandler');
    return this.handler;
  }
}

suite('TrackedElementTest', function() {
  let manager: TrackedElementManager;
  let handler: MockTrackedElementHandler;
  let element: HTMLElement;
  const NATIVE_ID = 'kElementId';

  /**
   * Waits for the current frame to render, which queues intersection events,
   * and then waits for the intersection events to propagate to listeners, which
   * triggers visibility messages.
   *
   * This takes a total of two frames. A single frame will cause the layout to
   * be updated, but will not actually propagate the events.
   */
  async function waitForVisibilityEvents() {
    await new Promise(resolve => requestAnimationFrame(resolve));
    await new Promise(resolve => requestAnimationFrame(resolve));
  }

  suiteSetup(() => {
    const proxy = new TestTrackedElementProxy();
    TrackedElementProxyImpl.setInstance(proxy);
    handler = proxy.handler;
    manager = TrackedElementManager.getInstance();
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement('div');
    element.id = 'element';
    element.style.width = '10px';
    element.style.height = '10px';
    document.body.appendChild(element);
  });

  teardown(() => {
    handler.reset();
    manager.reset();
  });

  test('startTracking sends visibility', async () => {
    manager.startTracking(element, NATIVE_ID);
    await waitForVisibilityEvents();
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertEquals(NATIVE_ID, args[0]);
    assertTrue(args[1]);  // visible
    const rect = element.getBoundingClientRect();
    assertDeepEquals(
        {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
        args[2]);
  });

  test('stopTracking sends visibility false', async () => {
    manager.startTracking(element, NATIVE_ID);
    await waitForVisibilityEvents();
    handler.reset();

    manager.stopTracking(element);
    // stopTracking sends visibility change synchronously.
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertEquals(NATIVE_ID, args[0]);
    assertFalse(args[1]);  // not visible
  });

  test('hiding element sends visibility false', async () => {
    manager.startTracking(element, NATIVE_ID);
    await waitForVisibilityEvents();
    handler.reset();

    element.style.display = 'none';
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertEquals(NATIVE_ID, args[0]);
    assertFalse(args[1]);  // not visible
  });

  test('showing element sends visibility true', async () => {
    element.style.display = 'none';
    document.body.appendChild(element);
    manager.startTracking(element, NATIVE_ID);
    await waitForVisibilityEvents();
    // Initially not visible
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    assertFalse(handler.getArgs('trackedElementVisibilityChanged')[0][1]);
    handler.reset();

    element.style.display = 'block';
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertEquals(NATIVE_ID, args[0]);
    assertTrue(args[1]);  // visible
  });

  test('fixed element tracking', async () => {
    manager.startTracking(element, NATIVE_ID, {fixed: true});
    await microtasksFinished();
    await waitForVisibilityEvents();
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertEquals(NATIVE_ID, args[0]);
    assertTrue(args[1]);  // visible
  });

  test('padding is applied to rect', async () => {
    manager.startTracking(
        element, NATIVE_ID,
        {paddingTop: 5, paddingLeft: 10, paddingBottom: 15, paddingRight: 20});
    await waitForVisibilityEvents();
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    const rect = element.getBoundingClientRect();
    assertDeepEquals(
        {
          x: rect.x - 10,
          y: rect.y - 5,
          width: rect.width + 10 + 20,
          height: rect.height + 5 + 15,
        },
        args[2]);
  });

  test('notifyElementActivated calls handler', () => {
    manager.startTracking(element, NATIVE_ID);
    manager.notifyElementActivated(element);
    assertEquals(1, handler.getCallCount('trackedElementActivated'));
    assertEquals(NATIVE_ID, handler.getArgs('trackedElementActivated')[0]);
  });

  test('notifyCustomEvent calls handler', () => {
    const eventName = 'test-event';
    manager.startTracking(element, NATIVE_ID);
    manager.notifyCustomEvent(element, eventName);
    assertEquals(1, handler.getCallCount('trackedElementCustomEvent'));
    assertEquals(NATIVE_ID, handler.getArgs('trackedElementCustomEvent')[0][0]);
    assertEquals(eventName, handler.getArgs('trackedElementCustomEvent')[0][1]);
  });
});
