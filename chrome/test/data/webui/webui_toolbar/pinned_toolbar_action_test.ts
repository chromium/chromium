// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';

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
    };
    TrackedElementManager.setInstanceForTesting(mockManager as any);

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
});
