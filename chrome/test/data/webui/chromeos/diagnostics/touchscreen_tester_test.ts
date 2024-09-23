// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/touchscreen_tester.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {DialogType, SCREEN_MAX_LENGTH, TouchEventType, TouchscreenTesterElement} from 'chrome://diagnostics/touchscreen_tester.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('touchscreenTesterTestSuite', function() {
  let touchscreenTesterElement: TouchscreenTesterElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    touchscreenTesterElement?.remove();
    touchscreenTesterElement = null;
  });


  function initializeTouchscreenTester(): Promise<void> {
    touchscreenTesterElement = document.createElement('touchscreen-tester');
    assert(touchscreenTesterElement);
    document.body.appendChild(touchscreenTesterElement);

    return flushTasks();
  }

  /**
   * openTester is a helper function for some boilerplate code. It opens the
   * intro dialog, clicks the start testing button and makes sure canvas dialog
   * is open. The function then returns the canvas dialog.
   */
  async function openTester(): Promise<CrDialogElement> {
    assert(touchscreenTesterElement);
    const introDialog = touchscreenTesterElement.getDialog(DialogType.INTRO);
    introDialog.showModal();
    await flushTasks();
    assertTrue(introDialog.open);

    const getStartedButton =
    strictQuery('cr-button', introDialog, CrButtonElement);
    getStartedButton.click();
    await flushTasks();
    assertFalse(introDialog.open);

    const canvasDialog = touchscreenTesterElement.getDialog(DialogType.CANVAS);
    assertTrue(canvasDialog.open);

    return canvasDialog;
  }

  test('OpenIntroDialog', async () => {
    await initializeTouchscreenTester();
    assert(touchscreenTesterElement);
    const introDialog = touchscreenTesterElement.getDialog(DialogType.INTRO);
    introDialog.showModal();
    await flushTasks();
    assertTrue(introDialog.open);
  });

  test('OpenCanvasDialog', async () => {
    await initializeTouchscreenTester();
    const canvasDialog = await openTester();

    const canvas = canvasDialog.querySelector('canvas');
    assert(canvas);
    assertEquals(canvas.width, SCREEN_MAX_LENGTH);
    assertEquals(canvas.height, SCREEN_MAX_LENGTH);
  });

  test('OnDrawStart', async () => {
    await initializeTouchscreenTester();
    assert(touchscreenTesterElement);
    await openTester();

    // Mock drawTrailMark and drawTrail function.
    const drawingProvider = touchscreenTesterElement.getDrawingProvider();
    const mockController = new MockController();
    const mockDrawTrailMark =
        mockController.createFunctionMock(drawingProvider, 'drawTrailMark');
    const mockDrawTrail =
        mockController.createFunctionMock(drawingProvider, 'drawTrail');

    const expectedTouches = new Map();
    const mockTouchEvents = [
      {
        id: 1,
        point: {x: 100, y: 150},
        pressure: 50,
      },
      {
        id: 2,
        point: {x: 500, y: 550},
        pressure: 30,
      },
    ];

    for (const {id, point, pressure} of mockTouchEvents) {
      // Add expected function call signature.
      mockDrawTrailMark.addExpectation(point.x, point.y);
      mockDrawTrail.addExpectation(
          point.x - 1, point.y, point.x, point.y, pressure);
      expectedTouches.set(id, point);
      touchscreenTesterElement.onDrawStart(id, point, pressure);

      assertDeepEquals(expectedTouches, touchscreenTesterElement.getTouches());
      mockController.verifyMocks();
    }
  });

  test('OnDraw', async () => {
    await initializeTouchscreenTester();
    assert(touchscreenTesterElement);
    await openTester();

    // Mock drawTrailMark and drawTrail function.
    const drawingProvider = touchscreenTesterElement.getDrawingProvider();
    const mockController = new MockController();
    const mockDrawTrail =
        mockController.createFunctionMock(drawingProvider, 'drawTrail');

    const expectedTouches = new Map();
    const mockTouchEvents = [
      {
        type: TouchEventType.START,
        id: 1,
        point: {x: 100, y: 150},
        pressure: 40,
      },
      {
        type: TouchEventType.MOVE,
        id: 1,
        point: {x: 110, y: 160},
        pressure: 30,
      },
    ];

    for (const {type, id, point, pressure} of mockTouchEvents) {
      if (type === TouchEventType.START) {
        mockDrawTrail.addExpectation(
            point.x - 1, point.y, point.x, point.y, pressure);
        touchscreenTesterElement.onDrawStart(id, point, pressure);
      } else if (type === TouchEventType.MOVE) {
        const previousPt = expectedTouches.get(id);
        mockDrawTrail.addExpectation(
            previousPt.x, previousPt.y, point.x, point.y, pressure);
        touchscreenTesterElement.onDraw(id, point, pressure);
      }

      expectedTouches.set(id, point);
      assertDeepEquals(expectedTouches, touchscreenTesterElement.getTouches());
      mockController.verifyMocks();
    }
  });

  test('OnDrawEnd', async () => {
    await initializeTouchscreenTester();
    assert(touchscreenTesterElement);
    await openTester();

    // Mock drawTrailMark and drawTrail function.
    const drawingProvider = touchscreenTesterElement.getDrawingProvider();
    const mockController = new MockController();
    const mockDrawTrailMark =
        mockController.createFunctionMock(drawingProvider, 'drawTrailMark');
    const mockDrawTrail =
        mockController.createFunctionMock(drawingProvider, 'drawTrail');

    const expectedTouches = new Map();
    const mockTouchEvents = [
      {
        type: TouchEventType.START,
        id: 1,
        point: {x: 100, y: 150},
        pressure: 40,
      },
      {
        type: TouchEventType.MOVE,
        id: 1,
        point: {x: 110, y: 160},
        pressure: 30,
      },
      {
        type: TouchEventType.END,
        id: 1,
        point: {x: 110, y: 160},
        pressure: 30,
      },
    ];

    for (const {type, id, point, pressure} of mockTouchEvents) {
      if (type === TouchEventType.START) {
        mockDrawTrailMark.addExpectation(point.x, point.y);
        mockDrawTrail.addExpectation(
            point.x - 1, point.y, point.x, point.y, pressure);
        touchscreenTesterElement.onDrawStart(id, point, pressure);
        expectedTouches.set(id, point);
      } else if (type === TouchEventType.MOVE) {
        const previousPt = expectedTouches.get(id);
        mockDrawTrail.addExpectation(
            previousPt.x, previousPt.y, point.x, point.y, pressure);
        touchscreenTesterElement.onDraw(id, point, pressure);
        expectedTouches.set(id, point);
      } else if (type === TouchEventType.END) {
        mockDrawTrailMark.addExpectation(point.x, point.y);
        touchscreenTesterElement.onDrawEnd(id, point);
        expectedTouches.delete(id);
      }

      assertDeepEquals(expectedTouches, touchscreenTesterElement.getTouches());
      mockController.verifyMocks();
    }
  });

  test('ObserveDataSource', async () => {
    await initializeTouchscreenTester();
    assert(touchscreenTesterElement);
    const canvasDialog = await openTester();
    const canvas = canvasDialog.querySelector('canvas');
    assert(canvas);
    const mockController = new MockController();

    // Create a mock touch.
    const touch = new Touch({
      identifier: 1,
      target: canvas,
      pageX: 300,
      pageY: 200,
      force: 0.3,
    });
    const expectedTouchPt = {
      x: touch.pageX - canvas.offsetLeft,
      y: touch.pageY - canvas.offsetTop,
    };

    for (const [eventType, mockFunctionName] of [
             [TouchEventType.START, 'onDrawStart'],
             [TouchEventType.MOVE, 'onDraw'],
             [TouchEventType.END, 'onDrawEnd']]) {
      const mockFunction = mockController.createFunctionMock(
          touchscreenTesterElement, mockFunctionName);
      if (mockFunctionName === 'onDrawEnd') {
        mockFunction.addExpectation(touch.identifier, expectedTouchPt);
      } else {
        mockFunction.addExpectation(
            touch.identifier, expectedTouchPt, touch.force);
      }
      assert(eventType);
      const touchEvent = eventToPromise(eventType, canvas);
      canvas.dispatchEvent(
          new TouchEvent(eventType, {changedTouches: [touch]}));
      await touchEvent;
      mockController.verifyMocks();
    }
  });
});
