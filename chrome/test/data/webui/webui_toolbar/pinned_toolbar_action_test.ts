// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import {DownloadProgressRingStatus} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api_data_model.mojom-webui.js';

suite('PinnedToolbarAction', function() {
  let action: any;
  let startTrackingCalls: Array<[HTMLElement, string]> = [];
  let stopTrackingCalls: HTMLElement[] = [];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    startTrackingCalls = [];
    stopTrackingCalls = [];

    const mockManager = {
      startTracking: (element: HTMLElement, nativeId: string) => {
        startTrackingCalls.push([element, nativeId]);
      },
      stopTracking: (element: HTMLElement) => {
        stopTrackingCalls.push(element);
      },
      notifyElementActivated: () => {},
    };
    TrackedElementManager.setInstance(mockManager as any);

    action = document.createElement('pinned-toolbar-action');
    // Set state before appending to avoid assertNotReached() in getIcon_()
    // during initial render.
    action.state = {
      ...action.state,
      action: 1,  // PinnedToolbarAction.kNewIncognitoWindow
      highlighted: false,
      enabled: true,
      elementId: null,
    };
    document.body.appendChild(action);
    await microtasksFinished();
  });

  test('Start tracking when elementId is set', async () => {
    action.state = {
      ...action.state,
      elementId: 'test-id',
    };
    await microtasksFinished();

    assertEquals(1, startTrackingCalls.length);
    assertEquals(action, startTrackingCalls[0]![0]);
    assertEquals('test-id', startTrackingCalls[0]![1]);
  });

  test('Stop tracking when elementId is cleared', async () => {
    action.state = {
      ...action.state,
      elementId: 'test-id',
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    action.state = {
      ...action.state,
      elementId: null,
    };
    await microtasksFinished();

    assertEquals(1, startTrackingCalls.length);
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(action, stopTrackingCalls[0]!);
  });

  test('Stop tracking and start tracking when elementId changes', async () => {
    action.state = {
      ...action.state,
      elementId: 'test-id-1',
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    action.state = {
      ...action.state,
      elementId: 'test-id-2',
    };
    await microtasksFinished();

    assertEquals(1, stopTrackingCalls.length);
    assertEquals(action, stopTrackingCalls[0]!);
    assertEquals(2, startTrackingCalls.length);
    assertEquals(action, startTrackingCalls[1]![0]);
    assertEquals('test-id-2', startTrackingCalls[1]![1]);
  });

  test('Stop tracking on disconnected', async () => {
    action.state = {
      ...action.state,
      elementId: 'test-id',
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    action.remove();
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(action, stopTrackingCalls[0]!);
  });

  test('Sets draggable attribute based on enabled state', async () => {
    const button = action.shadowRoot!.querySelector('cr-icon-button');
    assertEquals('true', button.getAttribute('draggable'));

    action.state = {
      ...action.state,
      enabled: false,
    };
    await microtasksFinished();
    assertEquals('false', button.getAttribute('draggable'));
  });

  test('Renders the progress ring', async () => {
    // `action` is untyped, so hold the shadow root in a typed local to be able
    // to use querySelector()'s type parameter.
    const shadowRoot: ShadowRoot = action.shadowRoot;
    const getRing = () =>
        shadowRoot.querySelector<SVGElement>('.progress-ring');
    const getFillDashArray = () =>
        getRing()!.querySelector<SVGElement>('.fill')!.style.strokeDasharray;

    // No ring is rendered unless the state asks for one.
    assertEquals(null, getRing());

    // DownloadProgressRingStatus.kDownloading
    action.state = {
      ...action.state,
      progressRing: {
        status: DownloadProgressRingStatus.kDownloading,
        progressPercentage: 40,
      },
    };
    await microtasksFinished();
    assertEquals('downloading', getRing()!.getAttribute('status'));
    assertEquals(40, parseFloat(getFillDashArray()));

    // DownloadProgressRingStatus.kScanning. The percentage doesn't apply to
    // indeterminate rings, so the dash pattern comes from the stylesheet.
    action.state = {
      ...action.state,
      progressRing: {
        status: DownloadProgressRingStatus.kScanning,
        progressPercentage: null,
      },
    };
    await microtasksFinished();
    assertEquals('scanning', getRing()!.getAttribute('status'));
    assertEquals('', getFillDashArray());

    // DownloadProgressRingStatus.kDormant
    action.state = {
      ...action.state,
      progressRing: {
        status: DownloadProgressRingStatus.kDormant,
        progressPercentage: null,
      },
    };
    await microtasksFinished();
    assertEquals('dormant', getRing()!.getAttribute('status'));

    action.state = {
      ...action.state,
      progressRing: null,
    };
    await microtasksFinished();
    assertEquals(null, getRing());
  });

  test('Sets draggable attribute based on poppedOut state', async () => {
    const button = action.shadowRoot!.querySelector('cr-icon-button')!;
    assertEquals('true', button.getAttribute('draggable'));

    action.poppedOut = true;
    await microtasksFinished();
    assertEquals('false', button.getAttribute('draggable'));

    // Verify dragstart is prevented when poppedOut is true.
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    button.dispatchEvent(dragStartEvent);
    assertTrue(dragStartEvent.defaultPrevented);
  });

  test('Keyboard left/right arrows move pinned action', () => {
    const movedByCalls: Array<[number, number]> = [];

    const mockHandler = {

      movePinnedToolbarActionBy: (actionId: number, delta: number) => {
        movedByCalls.push([actionId, delta]);
      },
      invokePinnedToolbarAction: () => {},
    };
    BrowserProxyImpl.setInstance({toolbarUIHandler: mockHandler} as any);

    const button = action.shadowRoot!.querySelector('cr-icon-button');
    button.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowLeft',
      ctrlKey: true,
      bubbles: true,
      composed: true,
    }));
    assertEquals(1, movedByCalls.length);
    assertEquals(1, movedByCalls[0]![0]);
    assertEquals(-1, movedByCalls[0]![1]);

    button.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowRight',
      ctrlKey: true,
      bubbles: true,
      composed: true,
    }));
    assertEquals(2, movedByCalls.length);
    assertEquals(1, movedByCalls[1]![0]);
    assertEquals(1, movedByCalls[1]![1]);
  });

  test('Sets aria-pressed based on highlighted state', async () => {
    const button = action.shadowRoot!.querySelector('cr-icon-button')!;
    assertEquals('false', button.getAttribute('aria-pressed'));

    action.state = {
      ...action.state,
      highlighted: true,
    };
    await microtasksFinished();
    assertEquals('true', button.getAttribute('aria-pressed'));

    action.state = {
      ...action.state,
      highlighted: false,
    };
    await microtasksFinished();
    assertEquals('false', button.getAttribute('aria-pressed'));

    action.trackedHighlighted = true;
    await microtasksFinished();
    assertEquals('true', button.getAttribute('aria-pressed'));

    action.trackedHighlighted = false;
    await microtasksFinished();
    assertEquals('false', button.getAttribute('aria-pressed'));
  });

  test('Sets preventOverflow based on highlighted state', async () => {
    assertFalse(action.preventOverflow);

    let layoutRequested = 0;
    action.addEventListener('request-layout', () => {
      layoutRequested++;
    });

    action.state = {
      ...action.state,
      highlighted: true,
    };
    await microtasksFinished();
    assertTrue(action.preventOverflow);
    assertEquals(1, layoutRequested);

    action.state = {
      ...action.state,
      highlighted: false,
    };
    await microtasksFinished();
    assertFalse(action.preventOverflow);
    assertEquals(2, layoutRequested);

    action.trackedHighlighted = true;
    await microtasksFinished();
    assertTrue(action.preventOverflow);
    assertEquals(3, layoutRequested);

    action.trackedHighlighted = false;
    await microtasksFinished();
    assertFalse(action.preventOverflow);
    assertEquals(4, layoutRequested);
  });

  test('Skips click execution when active bubble is highlighted', async () => {
    let invokeCalls = 0;
    const mockHandler = {
      invokePinnedToolbarAction: () => {
        invokeCalls++;
      },
    };
    BrowserProxyImpl.setInstance({toolbarUIHandler: mockHandler} as any);

    const button = action.shadowRoot!.querySelector('cr-icon-button')!;
    // `HighlightTracker` initializes `lastUnhighlightedTime` to 0 and skips
    // clicks when `performance.now() - lastUnhighlightedTime < 100`. Ensure the
    // initial click is outside that 100ms suppression window even if the test
    // runs within the first 100ms of page load.
    action.highlightTracker.lastUnhighlightedTime = performance.now() - 200;

    // When not highlighted, pointerdown + click invokes the action.
    button.dispatchEvent(new PointerEvent(
        'pointerdown', {button: 0, pointerType: 'mouse', bubbles: true}));
    button.dispatchEvent(new PointerEvent(
        'click', {button: 0, pointerType: 'mouse', bubbles: true}));
    assertEquals(1, invokeCalls);

    // When highlightTracker is highlighted on pointerdown, the following click
    // is skipped.
    action.highlightTracker.onHighlightChanged(true);
    action.trackedHighlighted = true;
    await microtasksFinished();
    button.dispatchEvent(new PointerEvent(
        'pointerdown', {button: 0, pointerType: 'mouse', bubbles: true}));
    button.dispatchEvent(new PointerEvent(
        'click', {button: 0, pointerType: 'mouse', bubbles: true}));
    assertEquals(1, invokeCalls);

    // Subsequent click when no longer highlighted invokes the action again.
    // Advance `lastUnhighlightedTime` past the 100ms post-close suppression
    // window that `onHighlightChanged(false)` starts at `performance.now()`.
    action.highlightTracker.onHighlightChanged(false);
    action.highlightTracker.lastUnhighlightedTime = performance.now() - 200;
    action.trackedHighlighted = false;
    await microtasksFinished();
    button.dispatchEvent(new PointerEvent(
        'pointerdown', {button: 0, pointerType: 'mouse', bubbles: true}));
    button.dispatchEvent(new PointerEvent(
        'click', {button: 0, pointerType: 'mouse', bubbles: true}));
    assertEquals(2, invokeCalls);
  });
});
