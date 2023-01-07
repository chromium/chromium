// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CanvasDrawingProvider, DESTINATION_OVER, LINE_CAP, LINE_WIDTH, MARK_COLOR, MARK_RADIUS, MAX_TOUCH_PRESSURE, TRAIL_COLOR, TRAIL_MAX_OPACITY} from 'chrome://diagnostics/drawing_provider.js';

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';

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

export function drawingProviderTestSuite() {
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
    const expectedStrokeStyle = `rgba(${TRAIL_COLOR}, ${
        TRAIL_MAX_OPACITY * (pressure / MAX_TOUCH_PRESSURE)})`;

    const drawingProvider = initializeDrawingProvider();
    drawingProvider.drawTrail(x0, y0, x1, y1, pressure);

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

    assertDeepEquals(expectedMock, drawingProvider.getCtx().getMock());
    assertEquals(MARK_COLOR, drawingProvider.getFillStyle());
    assertEquals(
        DESTINATION_OVER, drawingProvider.getGlobalCompositeOperation());
  });
}
