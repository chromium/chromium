// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
import type {TrackedElementProxy} from 'chrome://resources/js/tracked_element/tracked_element_proxy.js';
import {TrackedElementProxyImpl} from 'chrome://resources/js/tracked_element/tracked_element_proxy.js';
import type {RectF} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {TrackedElementHandlerInterface, TrackedElementIdentifier, TrackedElementManagerRemote} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {TrackedElementManagerCallbackRouter} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class MockTrackedElementHandler extends TestBrowserProxy implements
    TrackedElementHandlerInterface {
  managerRemote?: TrackedElementManagerRemote;

  constructor() {
    super([
      'setManager',
      'trackedElementVisibilityChanged',
      'trackedElementActivated',
      'trackedElementCustomEvent',
      'trackedElementCanHighlightChanged',
    ]);
  }

  setManager(managerRemote: TrackedElementManagerRemote) {
    this.methodCalled('setManager');
    this.managerRemote = managerRemote;
  }

  trackedElementVisibilityChanged(
      id: TrackedElementIdentifier, visible: boolean, rect: RectF) {
    this.methodCalled('trackedElementVisibilityChanged', id, visible, rect);
  }

  trackedElementActivated(id: TrackedElementIdentifier) {
    this.methodCalled('trackedElementActivated', id);
  }

  trackedElementCustomEvent(id: TrackedElementIdentifier, eventName: string) {
    this.methodCalled('trackedElementCustomEvent', id, eventName);
  }

  trackedElementCanHighlightChanged(
      id: TrackedElementIdentifier, canHighlight: boolean) {
    this.methodCalled('trackedElementCanHighlightChanged', id, canHighlight);
  }
}

class TestTrackedElementProxy extends TestBrowserProxy implements
    TrackedElementProxy {
  handler = new MockTrackedElementHandler();
  callbackRouter: TrackedElementManagerCallbackRouter =
      new TrackedElementManagerCallbackRouter();

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
  let element2: HTMLElement;
  let managerRemote: TrackedElementManagerRemote;
  const ELEMENT_ID: TrackedElementIdentifier = {
    nativeIdentifier: 'kElementId',
    secondaryIdentifier: '3',
  };
  const OTHER_ELEMENT_SAME_ID: TrackedElementIdentifier = {
    nativeIdentifier: 'kElementId',
    secondaryIdentifier: '5',
  };
  const NOT_ELEMENT_ID: TrackedElementIdentifier = {
    nativeIdentifier: 'kElementId2',
    secondaryIdentifier: '7',
  };

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
    // TrackedElementManager must have called setManager.
    assertEquals(1, handler.getCallCount('setManager'));
    assertTrue(!!handler.managerRemote);
    managerRemote = handler.managerRemote;
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement('div');
    element.id = 'element';
    element.style.width = '10px';
    element.style.height = '10px';
    document.body.appendChild(element);

    element2 = document.createElement('div');
    element2.id = 'element2';
    element2.style.width = '10px';
    element2.style.height = '10px';
    document.body.appendChild(element2);
  });

  teardown(() => {
    handler.reset();
    manager.reset();
  });

  test('startTracking sends visibility', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    assertEquals(0, handler.getCallCount('trackedElementCanHighlightChanged'));
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertTrue(args[1]);  // visible
    const rect = element.getBoundingClientRect();
    assertDeepEquals(
        {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
        args[2]);
  });

  test('stopTracking sends visibility false', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    manager.stopTracking(element);
    // stopTracking sends visibility change synchronously.
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    assertEquals(0, handler.getCallCount('trackedElementCanHighlightChanged'));
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertFalse(args[1]);  // not visible
  });

  test('startTracking w/highlight callback', async () => {
    const state: boolean[] = [];
    manager.startTracking(element, ELEMENT_ID.nativeIdentifier, {
      secondaryId: ELEMENT_ID.secondaryIdentifier,
      onHighlightChanged: (highlighted: boolean) => {
        state.push(highlighted);
      },
    });
    await waitForVisibilityEvents();
    // Should enable highlight support.
    assertEquals(1, handler.getCallCount('trackedElementCanHighlightChanged'));
    const args = handler.getArgs('trackedElementCanHighlightChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertTrue(args[1]);  // can highlight

    // No highlight changes yet.
    assertArrayEquals([], state);

    // Different ID doesn't do anything.
    managerRemote.onElementHighlightChanged(NOT_ELEMENT_ID, true);
    await managerRemote.$.flushForTesting();
    assertArrayEquals([], state);

    // Try with real ID.
    managerRemote.onElementHighlightChanged(ELEMENT_ID, true);
    managerRemote.onElementHighlightChanged(ELEMENT_ID, false);
    await managerRemote.$.flushForTesting();
    assertArrayEquals([true, false], state);
  });

  test('stopTracking w/highlight callback', async () => {
    const state: boolean[] = [];
    manager.startTracking(element, ELEMENT_ID.nativeIdentifier, {
      secondaryId: ELEMENT_ID.secondaryIdentifier,
      onHighlightChanged: (highlighted: boolean) => {
        state.push(highlighted);
      },
    });
    await waitForVisibilityEvents();
    handler.reset();

    manager.stopTracking(element);
    // Should disable highlight support.
    assertEquals(1, handler.getCallCount('trackedElementCanHighlightChanged'));
    const args = handler.getArgs('trackedElementCanHighlightChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertFalse(args[1]);  // can't highlight

    // Events after stopTracking are ignored.
    managerRemote.onElementHighlightChanged(ELEMENT_ID, true);
    managerRemote.onElementHighlightChanged(ELEMENT_ID, false);
    await managerRemote.$.flushForTesting();
    assertArrayEquals([], state);
  });

  test('hiding element sends visibility false', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    element.style.display = 'none';
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertFalse(args[1]);  // not visible
  });

  test('showing element sends visibility true', async () => {
    element.style.display = 'none';
    document.body.appendChild(element);
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    // Initially not visible
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    assertFalse(handler.getArgs('trackedElementVisibilityChanged')[0][1]);
    handler.reset();

    element.style.display = 'block';
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertTrue(args[1]);  // visible
  });

  test('Adding hidden attribute sends visibility false', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    element.hidden = true;
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertFalse(args[1]);  // not visible
  });

  test('Removing hidden attribute sends visibility false', async () => {
    element.hidden = true;
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    element.hidden = false;
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertTrue(args[1]);  // visible
  });

  test('fixed element tracking', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier, fixed: true});
    await microtasksFinished();
    await waitForVisibilityEvents();
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged')[0];
    assertDeepEquals(ELEMENT_ID, args[0]);
    assertTrue(args[1]);  // visible
  });

  test('padding is applied to rect', async () => {
    manager.startTracking(element, ELEMENT_ID.nativeIdentifier, {
      secondaryId: ELEMENT_ID.secondaryIdentifier,
      paddingTop: 5,
      paddingLeft: 10,
      paddingBottom: 15,
      paddingRight: 20,
    });
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
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    manager.notifyElementActivated(element);
    assertEquals(1, handler.getCallCount('trackedElementActivated'));
    assertDeepEquals(ELEMENT_ID, handler.getArgs('trackedElementActivated')[0]);
  });

  test('notifyCustomEvent calls handler', () => {
    const eventName = 'test-event';
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    manager.notifyCustomEvent(element, eventName);
    assertEquals(1, handler.getCallCount('trackedElementCustomEvent'));
    assertDeepEquals(
        ELEMENT_ID, handler.getArgs('trackedElementCustomEvent')[0][0]);
    assertEquals(eventName, handler.getArgs('trackedElementCustomEvent')[0][1]);
  });

  test('moving element in DOM sends visibility update', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    // Move element to a different parent
    const newParent = document.createElement('div');
    newParent.style.marginLeft = '50px';
    document.body.appendChild(newParent);
    newParent.appendChild(element);
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged');
    // Should have been called when removed and when added
    assertGT(args.length, 0);
    // Last call should show new position
    const lastCall = args[args.length - 1];
    assertDeepEquals(ELEMENT_ID, lastCall[0]);
    assertTrue(lastCall[1]);  // visible
    const rect = element.getBoundingClientRect();
    assertDeepEquals(
        {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
        lastCall[2]);
  });

  test('changing element style sends visibility update', async () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    // Change element's position via style
    element.style.position = 'relative';
    element.style.left = '100px';
    element.style.top = '50px';
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged');
    const lastCall = args[args.length - 1];
    assertDeepEquals(ELEMENT_ID, lastCall[0]);
    assertTrue(lastCall[1]);  // visible
    const rect = element.getBoundingClientRect();
    assertDeepEquals(
        {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
        lastCall[2]);
  });

  test('changing element class sends visibility update', async () => {
    // Add a style for a class that changes position
    const style = document.createElement('style');
    style.textContent = '.moved { margin-left: 75px; }';
    document.head.appendChild(style);

    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();
    handler.reset();

    element.classList.add('moved');
    await waitForVisibilityEvents();

    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    const args = handler.getArgs('trackedElementVisibilityChanged');
    const lastCall = args[args.length - 1];
    assertDeepEquals(ELEMENT_ID, lastCall[0]);
    assertTrue(lastCall[1]);  // visible

    document.head.removeChild(style);
  });

  test('tracking detached element then adding to DOM', async () => {
    // Create a detached element (not in the DOM tree)
    const detachedElement = document.createElement('div');
    detachedElement.id = 'detached';
    detachedElement.style.width = '20px';
    detachedElement.style.height = '20px';

    // Start tracking the detached element
    manager.startTracking(
        detachedElement, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();

    // Element should not be visible initially
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    let args = handler.getArgs('trackedElementVisibilityChanged');
    assertDeepEquals(ELEMENT_ID, args[0][0]);
    assertFalse(args[0][1]);  // not visible
    handler.reset();

    // Add the element to the DOM
    document.body.appendChild(detachedElement);
    await waitForVisibilityEvents();

    // Element should now be visible
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    args = handler.getArgs('trackedElementVisibilityChanged');
    const lastCall = args[args.length - 1];
    assertDeepEquals(ELEMENT_ID, lastCall[0]);
    assertTrue(lastCall[1]);  // visible
    const rect = detachedElement.getBoundingClientRect();
    assertDeepEquals(
        {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
        lastCall[2]);
  });

  test('tracking detached element in subtree then adding to DOM', async () => {
    // Create a parent with a child, both detached
    const detachedParent = document.createElement('div');
    const detachedChild = document.createElement('div');
    detachedChild.id = 'detached-child';
    detachedChild.style.width = '15px';
    detachedChild.style.height = '15px';
    detachedParent.appendChild(detachedChild);

    // Start tracking the detached child
    manager.startTracking(
        detachedChild, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();

    // Element should not be visible initially
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    let args = handler.getArgs('trackedElementVisibilityChanged');
    assertDeepEquals(ELEMENT_ID, args[0][0]);
    assertFalse(args[0][1]);  // not visible
    handler.reset();

    // Add the parent (and implicitly the child) to the DOM
    document.body.appendChild(detachedParent);
    await waitForVisibilityEvents();

    // Child element should now be visible
    assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 0);
    args = handler.getArgs('trackedElementVisibilityChanged');
    const lastCall = args[args.length - 1];
    assertDeepEquals(ELEMENT_ID, lastCall[0]);
    assertTrue(lastCall[1]);  // visible
    const rect = detachedChild.getBoundingClientRect();
    assertDeepEquals(
        {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
        lastCall[2]);
  });

  test('clickElement_ waits until not disabled', async () => {
    const button = document.createElement('button');
    button.id = 'button';
    document.body.appendChild(button);

    let clicked = false;
    button.addEventListener('click', () => {
      clicked = true;
    });

    manager.startTracking(
        button, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    await waitForVisibilityEvents();

    // Disable the button and try to click it.
    button.disabled = true;
    const clickPromise = managerRemote.clickElement(ELEMENT_ID);

    // Give it some time to make sure it hasn't clicked.
    await microtasksFinished();
    assertFalse(clicked);

    // Enable the button and wait for the click to complete.
    button.disabled = false;
    const result = await clickPromise;
    assertTrue(result.success);
    assertTrue(clicked);
  });

  // Tests for multiple elements with the same native identifier:

  test(
      'startTracking two elements with same native ID sends visibility',
      async () => {
        manager.startTracking(
            element, ELEMENT_ID.nativeIdentifier,
            {secondaryId: ELEMENT_ID.secondaryIdentifier});
        manager.startTracking(
            element2, OTHER_ELEMENT_SAME_ID.nativeIdentifier,
            {secondaryId: OTHER_ELEMENT_SAME_ID.secondaryIdentifier});
        await waitForVisibilityEvents();
        assertGT(handler.getCallCount('trackedElementVisibilityChanged'), 1);
        assertEquals(
            0, handler.getCallCount('trackedElementCanHighlightChanged'));

        const allArgs = handler.getArgs('trackedElementVisibilityChanged');
        const args = allArgs.find(
            a => a[0].secondaryIdentifier === ELEMENT_ID.secondaryIdentifier)!;
        assertDeepEquals(ELEMENT_ID, args[0]);
        assertTrue(args[1]);  // visible
        const rect = element.getBoundingClientRect();
        assertDeepEquals(
            {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
            args[2]);

        const args2 = allArgs.find(
            a => a[0].secondaryIdentifier ===
                OTHER_ELEMENT_SAME_ID.secondaryIdentifier)!;
        assertDeepEquals(OTHER_ELEMENT_SAME_ID, args2[0]);
        assertTrue(args2[1]);  // visible
        const rect2 = element2.getBoundingClientRect();
        assertDeepEquals(
            {x: rect2.x, y: rect2.y, width: rect2.width, height: rect2.height},
            args2[2]);

        assertEquals(
            manager.getTrackedElement(element),
            manager.getTrackedElementById(ELEMENT_ID));
        assertEquals(
            manager.getTrackedElement(element2),
            manager.getTrackedElementById(OTHER_ELEMENT_SAME_ID));
        assertNotEquals(
            manager.getTrackedElement(element),
            manager.getTrackedElement(element2));
      });

  test(
      'stopTracking only makes one element with the same ID go away',
      async () => {
        manager.startTracking(
            element, ELEMENT_ID.nativeIdentifier,
            {secondaryId: ELEMENT_ID.secondaryIdentifier});
        manager.startTracking(
            element2, OTHER_ELEMENT_SAME_ID.nativeIdentifier,
            {secondaryId: OTHER_ELEMENT_SAME_ID.secondaryIdentifier});
        await waitForVisibilityEvents();
        manager.stopTracking(element);
        assertEquals(undefined, manager.getTrackedElement(element));
        assertEquals(undefined, manager.getTrackedElementById(ELEMENT_ID));
        assertNotEquals(undefined, manager.getTrackedElement(element2));
        assertNotEquals(
            undefined, manager.getTrackedElementById(OTHER_ELEMENT_SAME_ID));
        assertEquals(
            manager.getTrackedElement(element2),
            manager.getTrackedElementById(OTHER_ELEMENT_SAME_ID));
      });

  test('notifyElementActivated calls handler for correct element', () => {
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    manager.startTracking(
        element2, OTHER_ELEMENT_SAME_ID.nativeIdentifier,
        {secondaryId: OTHER_ELEMENT_SAME_ID.secondaryIdentifier});

    manager.notifyElementActivated(element);
    assertEquals(1, handler.getCallCount('trackedElementActivated'));
    assertDeepEquals(ELEMENT_ID, handler.getArgs('trackedElementActivated')[0]);

    manager.notifyElementActivated(element2);
    assertEquals(2, handler.getCallCount('trackedElementActivated'));
    assertDeepEquals(
        OTHER_ELEMENT_SAME_ID, handler.getArgs('trackedElementActivated')[1]);
  });

  test('notifyCustomEvent calls handler for correct element', () => {
    const eventName = 'test-event';
    manager.startTracking(
        element, ELEMENT_ID.nativeIdentifier,
        {secondaryId: ELEMENT_ID.secondaryIdentifier});
    manager.startTracking(
        element2, OTHER_ELEMENT_SAME_ID.nativeIdentifier,
        {secondaryId: OTHER_ELEMENT_SAME_ID.secondaryIdentifier});

    manager.notifyCustomEvent(element, eventName);
    assertEquals(1, handler.getCallCount('trackedElementCustomEvent'));
    assertDeepEquals(
        ELEMENT_ID, handler.getArgs('trackedElementCustomEvent')[0][0]);
    assertEquals(eventName, handler.getArgs('trackedElementCustomEvent')[0][1]);

    manager.notifyCustomEvent(element2, eventName);
    assertEquals(2, handler.getCallCount('trackedElementCustomEvent'));
    assertDeepEquals(
        OTHER_ELEMENT_SAME_ID,
        handler.getArgs('trackedElementCustomEvent')[1][0]);
    assertEquals(eventName, handler.getArgs('trackedElementCustomEvent')[1][1]);
  });
});
