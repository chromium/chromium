// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-fingerprint-progress-arc. */
cr.define('cr_fingerprint_progress_arc_test', function() {
  /**
   * An object descrbing a 2d point.
   * @typedef {{
   *   x: number,
   *   y: number,
   * }}
   */
  let Point;

  /**
   * An object descrbing a color with r, g and b values.
   * @typedef {{
   *   r: number,
   *   g: number,
   *   b: number,
   * }}
   */
  let Color;

  /** @type {?SettingsFingerprintProgressArcElement} */
  let progressArc = null;

  /** @type {?HTMLCanvasElement} */
  let canvas = null;

  /** @type {Color} */
  const black = {r: 0, g: 0, b: 0};
  /** @type {Color} */
  const blue = {r: 0, g: 0, b: 255};
  /** @type {Color} */
  const white = {r: 255, g: 255, b: 255};

  setup(function() {
    PolymerTest.clearBody();
    progressArc = document.createElement('cr-fingerprint-progress-arc');
    document.body.appendChild(progressArc);

    // Override some parameters and function for testing purposes.
    canvas = progressArc.$.canvas;
    canvas.width = 300;
    canvas.height = 150;
    progressArc.circleRadius = 50;
    progressArc.canvasCircleStrokeWidth_ = 3;
    progressArc.canvasCircleBackgroundColor_ = 'rgba(0,0,0,1.0)';
    progressArc.canvasCircleProgressColor_ = 'rgba(0,0,255,1.0)';
    progressArc.canvasCircleShadowColor_ = 'rgba(0,0,0,1.0)';
    progressArc.clearCanvas = function() {
      const ctx = canvas.getContext('2d');
      ctx.fillStyle = 'rgba(255,255,255,1.0)';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
    };
    progressArc.clearCanvas();
    Polymer.dom.flush();
  });

  /**
   * Helper function which gets the rgb values at |point| on the canvas.
   * @param {Point} point
   * @return {Color}
   */
  function getRGBData(point) {
    const ctx = canvas.getContext('2d');
    const pixel = ctx.getImageData(point.x, point.y, 1, 1).data;
    return {r: pixel[0], g: pixel[1], b: pixel[2]};
  }

  /**
   * Helper function which checks if the given color is matches the expected
   * color.
   * @param {Color} expectedColor
   * @param {Color} actualColor
   */
  function assertColorEquals(expectedColor, actualColor) {
    assertEquals(expectedColor.r, actualColor.r);
    assertEquals(expectedColor.g, actualColor.g);
    assertEquals(expectedColor.b, actualColor.b);
  }

  /**
   * Helper function which checks that a list of points match the color the are
   * expected to have on the canvas.
   * @param {Color} expectedColor
   * @param {!Array<Point>} listOfPoints
   */
  function assertListOfColorsEqual(expectedColor, listOfPoints) {
    for (const point of listOfPoints) {
      assertColorEquals(expectedColor, getRGBData(point));
    }
  }

  test('TestDrawArc', function() {
    // Verify that by drawing an arc from 0 to PI/2 with radius 50 and center at
    // (150, 75), points along that arc should be blue, and points not on that
    // arc should remain white.
    progressArc.drawArc(0, Math.PI / 2, progressArc.canvasCircleProgressColor_);
    /** @type {Array<Point>} */
    let expectedPointsOnArc = [
      {x: 200, y: 75} /* 0rad */, {x: 185, y: 110} /* PI/4rad */,
      {x: 150, y: 125} /* PI/2rad */
    ];
    /** @type {Array<Point>} */
    let expectedPointsNotOnArc =
        [{x: 115, y: 110} /* 3PI/4rad */, {x: 100, y: 75} /* PI */];
    assertListOfColorsEqual(blue, expectedPointsOnArc);
    assertListOfColorsEqual(white, expectedPointsNotOnArc);

    // After clearing, the points that were blue should be white.
    progressArc.clearCanvas();
    assertListOfColorsEqual(white, expectedPointsOnArc);

    // Verify that by drawing an arc from 3PI/2 to 5PI/2 with radius 50 and
    // center at (150, 75), points along that arc should be blue, and points not
    // on that arc should remain white.
    progressArc.drawArc(
        3 * Math.PI / 2, 5 * Math.PI / 2,
        progressArc.canvasCircleProgressColor_);
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
    /** @type {Array<Point>} */
    const expectedPointsInCircle = [
      {x: 200, y: 75} /* 0rad */, {x: 150, y: 125} /* PI/2rad */,
      {x: 100, y: 75} /* PIrad */, {x: 150, y: 25} /* 3PI/2rad */
    ];
    /** @type {Array<Point>} */
    const expectedPointsNotInCircle = [
      {x: 110, y: 75} /* Too left, outside of stroke */,
      {x: 90, y: 75} /* Too right, inside of stroke */,
      {x: 200, y: 100} /* Outside of circle */,
      {x: 150, y: 75} /* In the center */
    ];
    assertListOfColorsEqual(black, expectedPointsInCircle);
    assertListOfColorsEqual(white, expectedPointsNotInCircle);

    // After clearing, the points that were black should be white.
    progressArc.clearCanvas();
    assertListOfColorsEqual(white, expectedPointsInCircle);
  });
});