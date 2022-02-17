// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';

import {CrFingerprintProgressArcElement, FINGERPRINT_TICK_DARK_URL, FINGERPRINT_TICK_LIGHT_URL} from 'chrome://resources/cr_elements/cr_fingerprint/cr_fingerprint_progress_arc.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
// clang-format on

class FakeMediaQueryList extends EventTarget implements MediaQueryList {
  private listener_: ((e: MediaQueryListEvent) => any)|null = null;
  private matches_: boolean = false;
  private media_: string;

  constructor(media: string) {
    super();
    this.media_ = media;
  }

  addListener(listener: (e: MediaQueryListEvent) => any) {
    this.listener_ = listener;
  }

  removeListener(listener: (e: MediaQueryListEvent) => any) {
    assertEquals(listener, this.listener_);
    this.listener_ = null;
  }

  onchange() {
    if (this.listener_) {
      this.listener_(new MediaQueryListEvent(
          'change', {media: this.media_, matches: this.matches_}));
    }
  }

  get media(): string {
    return this.media_;
  }

  get matches(): boolean {
    return this.matches_;
  }

  set matches(matches: boolean) {
    if (this.matches_ !== matches) {
      this.matches_ = matches;
      this.onchange();
    }
  }
}

/** @fileoverview Suite of tests for cr-fingerprint-progress-arc. */
suite('cr_fingerprint_progress_arc_test', function() {
  /**
   * An object descrbing a 2d point.
   */
  type Point = {
    x: number,
    y: number,
  };

  /**
   * An object descrbing a color with r, g and b values.
   */
  type Color = {
    r: number,
    g: number,
    b: number,
  }

  let progressArc: CrFingerprintProgressArcElement;
  let canvas: HTMLCanvasElement;

  const black: Color = {r: 0, g: 0, b: 0};
  const blue: Color = {r: 0, g: 0, b: 255};
  const white: Color = {r: 255, g: 255, b: 255};

  let mockController: MockController;
  let fakeMediaQueryList: FakeMediaQueryList;

  function clearCanvas(canvas: HTMLCanvasElement) {
    const ctx = canvas.getContext('2d')!;
    ctx.fillStyle = 'rgba(255,255,255,1.0)';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }

  setup(function() {
    mockController = new MockController();
    const matchMediaMock =
        mockController.createFunctionMock(window, 'matchMedia');
    matchMediaMock.addExpectation('(prefers-color-scheme: dark)');
    fakeMediaQueryList = new FakeMediaQueryList('(prefers-color-scheme: dark)');
    matchMediaMock.returnValue = fakeMediaQueryList;

    document.body.innerHTML = '';
    progressArc = /** @type {!CrFingerprintProgressArcElement} */ (
        document.createElement('cr-fingerprint-progress-arc'));
    document.body.appendChild(progressArc);

    // Override some parameters and function for testing purposes.
    canvas = progressArc.$.canvas;
    canvas.width = 300;
    canvas.height = 150;
    progressArc.circleRadius = 50;
    progressArc.canvasCircleStrokeWidth = 3;
    progressArc.canvasCircleBackgroundColor = 'rgba(0,0,0,1.0)';
    progressArc.canvasCircleProgressColor = 'rgba(0,0,255,1.0)';
    clearCanvas(progressArc.$.canvas);
    flush();
  });

  teardown(function() {
    mockController.reset();
  });

  /**
   * Helper function which gets the rgb values at |point| on the canvas.
   */
  function getRGBData(point: Point): Color {
    const ctx = canvas.getContext('2d')!;
    const pixel = ctx.getImageData(point.x, point.y, 1, 1).data;
    return {r: pixel[0]!, g: pixel[1]!, b: pixel[2]!};
  }

  /**
   * Helper function which checks if the given color is matches the expected
   * color.
   */
  function assertColorEquals(expectedColor: Color, actualColor: Color) {
    assertEquals(expectedColor.r, actualColor.r);
    assertEquals(expectedColor.g, actualColor.g);
    assertEquals(expectedColor.b, actualColor.b);
  }

  /**
   * Helper function which checks that a list of points match the color the are
   * expected to have on the canvas.
   */
  function assertListOfColorsEqual(
      expectedColor: Color, listOfPoints: Point[]) {
    for (const point of listOfPoints) {
      assertColorEquals(expectedColor, getRGBData(point));
    }
  }

  test('TestDrawArc', function() {
    // Verify that by drawing an arc from 0 to PI/2 with radius 50 and center at
    // (150, 75), points along that arc should be blue, and points not on that
    // arc should remain white.
    progressArc.drawArc(0, Math.PI / 2, progressArc.canvasCircleProgressColor);
    let expectedPointsOnArc = [
      {x: 200, y: 75} /* 0rad */, {x: 185, y: 110} /* PI/4rad */,
      {x: 150, y: 125} /* PI/2rad */
    ];
    let expectedPointsNotOnArc =
        [{x: 115, y: 110} /* 3PI/4rad */, {x: 100, y: 75} /* PI */];
    assertListOfColorsEqual(blue, expectedPointsOnArc);
    assertListOfColorsEqual(white, expectedPointsNotOnArc);

    // After clearing, the points that were blue should be white.
    clearCanvas(progressArc.$.canvas);
    assertListOfColorsEqual(white, expectedPointsOnArc);

    // Verify that by drawing an arc from 3PI/2 to 5PI/2 with radius 50 and
    // center at (150, 75), points along that arc should be blue, and points not
    // on that arc should remain white.
    progressArc.drawArc(
        3 * Math.PI / 2, 5 * Math.PI / 2,
        progressArc.canvasCircleProgressColor);
    expectedPointsOnArc = [
      {x: 150, y: 25} /* 3PI/2 */, {x: 185, y: 40} /* 7PI/4 */,
      {x: 200, y: 75} /* 2PI */, {x: 185, y: 110} /* 9PI/4 */,
      {x: 150, y: 125} /* 5PI/2rad */
    ];
    expectedPointsNotOnArc = [
      {x: 115, y: 110} /* 3PI/4rad */, {x: 100, y: 75} /* PI */, {x: 115, y: 40}
      /* 5PI/4 */
    ];
    assertListOfColorsEqual(blue, expectedPointsOnArc);
    assertListOfColorsEqual(white, expectedPointsNotOnArc);
  });

  test('TestDrawBackgroundCircle', function() {
    // Verify that by drawing an circle with radius 50 and center at (150, 75),
    // points along that arc should be black, and points not on that arc should
    // remain white.
    progressArc.drawBackgroundCircle();
    const expectedPointsInCircle = [
      {x: 200, y: 75} /* 0rad */, {x: 150, y: 125} /* PI/2rad */,
      {x: 100, y: 75} /* PIrad */, {x: 150, y: 25} /* 3PI/2rad */
    ];
    const expectedPointsNotInCircle = [
      {x: 110, y: 75} /* Too left, outside of stroke */,
      {x: 90, y: 75} /* Too right, inside of stroke */,
      {x: 200, y: 100} /* Outside of circle */,
      {x: 150, y: 75} /* In the center */
    ];
    assertListOfColorsEqual(black, expectedPointsInCircle);
    assertListOfColorsEqual(white, expectedPointsNotInCircle);

    // After clearing, the points that were black should be white.
    clearCanvas(progressArc.$.canvas);
    assertListOfColorsEqual(white, expectedPointsInCircle);
  });

  test('TestSwitchToDarkMode', function() {
    const scanningAnimation = progressArc.$.scanningAnimation;
    progressArc.setProgress(0, 1, true);
    assertEquals(FINGERPRINT_TICK_LIGHT_URL, scanningAnimation.animationUrl);

    fakeMediaQueryList.matches = true;
    assertEquals(FINGERPRINT_TICK_DARK_URL, scanningAnimation.animationUrl);
  });
});
