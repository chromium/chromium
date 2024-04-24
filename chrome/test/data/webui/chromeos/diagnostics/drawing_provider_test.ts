// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {CanvasDrawingProvider} from 'chrome://diagnostics/drawing_provider.js';
import {constructRgba, DESTINATION_OVER, LINE_CAP, LINE_WIDTH, lookupCssVariableValue, MARK_COLOR, MARK_OPACITY, MARK_RADIUS, TRAIL_COLOR, TRAIL_MAX_OPACITY} from 'chrome://diagnostics/drawing_provider_utils.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';

/**
 * FakeCanvasCtx class mocks various html Canvas API methods to make it easy to
 * test the drawing behavior.
 */
class FakeCanvasCtx implements Partial<CanvasRenderingContext2D> {
  mock: string[] = [];

  getMock(): string[] {
    return this.mock;
  }

  beginPath(): void {
    this.mock.push('beginPath');
  }

  stroke(): void {
    this.mock.push('stroke');
  }

  fill(): void {
    this.mock.push('fill');
  }

  moveTo(x: number, y: number): void {
    this.mock.push(`moveTo:${x}~${y}`);
  }

  lineTo(x: number, y: number): void {
    this.mock.push(`lineTo:${x}~${y}`);
  }

  arc(x: number, y: number, r: number, startAngle: number,
      endAngle: number): void {
    this.mock.push(`arc:${x}~${y}~${r}~${startAngle}~${endAngle}`);
  }
}

suite('drawingProviderTestSuite', function() {
  const mockController = new MockController();

  setup(() => {
    // Setup mock for window.getComputedStyle function to prevent test flaky.
    const mockComputedStyle =
        mockController.createFunctionMock(window, 'getComputedStyle');
    mockComputedStyle.returnValue = {
      getPropertyValue: (valName: string) => {
        switch (valName) {
          case TRAIL_COLOR:
            return 'rgb(220, 210, 155)';
          case MARK_COLOR:
            return 'rgb(198, 179, 165)';
          case MARK_OPACITY:
            return '0.7';
          default:
            assertNotReached();
        }
      },
    };
  });

  teardown(() => {
    mockController.reset();
  });

  function initializeDrawingProvider(): CanvasDrawingProvider {
    return new CanvasDrawingProvider(
        new FakeCanvasCtx() as unknown as CanvasRenderingContext2D);
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
        lookupCssVariableValue(TRAIL_COLOR), `${TRAIL_MAX_OPACITY * pressure}`);

    assertDeepEquals(
        expectedMock,
        (drawingProvider.getCtx() as unknown as FakeCanvasCtx).getMock());
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

    assertDeepEquals(
        expectedMock,
        (drawingProvider.getCtx() as unknown as FakeCanvasCtx).getMock());
    assertEquals(expectedFillStyle, drawingProvider.getFillStyle());
    assertEquals(
        DESTINATION_OVER, drawingProvider.getGlobalCompositeOperation());
  });
});
