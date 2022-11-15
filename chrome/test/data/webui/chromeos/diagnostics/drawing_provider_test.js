// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {CanvasDrawingProvider} from 'chrome://diagnostics/drawing_provider.js';
import {constructRgba, DESTINATION_OVER, LINE_CAP, LINE_WIDTH, lookupCssVariableValue, MARK_COLOR, MARK_OPACITY, MARK_RADIUS, TRAIL_COLOR, TRAIL_MAX_OPACITY} from 'chrome://diagnostics/drawing_provider_utils.js';

import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from '../mock_controller.m.js';

/**
 * FakeCanvasCtx class mocks various html Canvas API methods to make it easy to
 * test the drawing behavior.
 */
class FakeCanvasCtx {
  constructor() {
    this.mock = [];
  }

  getMock() {
    return this.mock;
  }

  beginPath() {
    this.mock.push('beginPath');
  }

  stroke() {
    this.mock.push('stroke');
  }

  fill() {
    this.mock.push('fill');
  }

  moveTo(x, y) {
    this.mock.push(`moveTo:${x}~${y}`);
  }

  lineTo(x, y) {
    this.mock.push(`lineTo:${x}~${y}`);
  }

  arc(x, y, r, startAngle, endAngle) {
    this.mock.push(`arc:${x}~${y}~${r}~${startAngle}~${endAngle}`);
  }
}

suite('drawingProviderTestSuite', function() {
  /** @type {{createFunctionMock: Function, reset: Function}} */
  let mockController;

  setup(() => {
    // Setup mock for window.getComputedStyle function to prevent test flaky.
    mockController = new MockController();
    const mockComputedStyle =
        mockController.createFunctionMock(window, 'getComputedStyle');
    mockComputedStyle.returnValue = {
      getPropertyValue: (valName) => {
        switch (valName) {
          case TRAIL_COLOR:
            return 'rgb(220, 210, 155)';
          case MARK_COLOR:
            return 'rgb(198, 179, 165)';
          case MARK_OPACITY:
            return '0.7';
        }
      },
    };
  });

  teardown(() => {
    mockController.reset();
  });

  function initializeDrawingProvider() {
    return new CanvasDrawingProvider(new FakeCanvasCtx());
  }

  test('SettingUpCanvasDrawingProvider', () => {
    const drawingProvider = initializeDrawingProvider();

    assertEquals(LINE_CAP, drawingProvider.getLineCap());
    assertEquals(LINE_WIDTH, drawingProvider.getLineWidth());
  });

  test('TestDrawTrail', () => {
    const x0 = 10;
    const y0 = 15;
    const x1 = 20;
    const y1 = 25;
    const pressure = 100;
    const expectedMock = [
      'beginPath',
      `moveTo:${x0}~${y0}`,
      `lineTo:${x1}~${y1}`,
      'stroke',
    ];

    const drawingProvider = initializeDrawingProvider();
    drawingProvider.drawTrail(x0, y0, x1, y1, pressure);

    const expectedStrokeStyle = constructRgba(
        lookupCssVariableValue(TRAIL_COLOR), TRAIL_MAX_OPACITY * pressure);

    assertDeepEquals(expectedMock, drawingProvider.getCtx().getMock());
    assertEquals(expectedStrokeStyle, drawingProvider.getStrokeStyle());
  });

  test('TestDrawTrailMark', () => {
    const x = 10;
    const y = 15;
    const expectedMock = [
      'beginPath',
      `arc:${x}~${y}~${MARK_RADIUS}~${0}~${2 * Math.PI}`,
      'fill',
    ];

    const drawingProvider = initializeDrawingProvider();
    drawingProvider.drawTrailMark(x, y);

    const expectedFillStyle = constructRgba(
        lookupCssVariableValue(MARK_COLOR),
        lookupCssVariableValue(MARK_OPACITY));

    assertDeepEquals(expectedMock, drawingProvider.getCtx().getMock());
    assertEquals(expectedFillStyle, drawingProvider.getFillStyle());
    assertEquals(
        DESTINATION_OVER, drawingProvider.getGlobalCompositeOperation());
  });
});
