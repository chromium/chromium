// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';

import {CrFingerprintProgressArcElement, FINGERPRINT_TICK_DARK_URL, FINGERPRINT_TICK_LIGHT_URL} from 'chrome://resources/cr_elements/cr_fingerprint/cr_fingerprint_progress_arc.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
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

  const canvasColor: Color = {r: 0, g: 0, b: 255};
  const canvasColorStr: string = 'rgba(0,0,255,1.0)';
  const circleBackgroundColor: Color = {r: 0, g: 255, b: 0};
  const circleBackgroundColorStr: string = 'rgba(0,255,0,1.0)';
  const circleProgressColor: Color = {r: 255, g: 0, b: 0};
  const circleProgressColorStr: string = 'rgba(255,0,0,1.0)';

  let mockController: MockController;
  let fakeMediaQueryList: FakeMediaQueryList;

  function clearCanvas(canvas: HTMLCanvasElement) {
    const ctx = canvas.getContext('2d')!;
    ctx.fillStyle = canvasColorStr;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }

  setup(function() {
    mockController = new MockController();
    const matchMediaMock =
        mockController.createFunctionMock(window, 'matchMedia');
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
    progressArc.canvasCircleBackgroundColor = circleBackgroundColorStr;
    progressArc.canvasCircleProgressColor = circleProgressColorStr;
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

  test('TestSetProgress', async () => {
    // Angles on HTML canvases start at 0 radians on the positive x-axis and
    // increase in the clockwise direction. Progress is drawn starting from the
    // top of the circle (3pi/2 rad). We will check the colors drawn at angles
    // 7pi/4, pi/4, 3pi/4, and 5pi/4 rad (respectively 12.5%, 37.5%, 62.5%, and
    // 87.5% progress completed).
    const checkPoints = [
      {x: 185, y: 40} /* 7pi/4 rad */, {x: 185, y: 110} /* pi/4 rad */,
      {x: 115, y: 110} /* 3pi/4 rad */, {x: 115, y: 40} /* 5pi/4 rad */
    ];

    const testCases = [
      // Verify that no progress is drawn when no progress has been made.
      {
        'setProgressArgs': {
          'prevPercentComplete': 0,
          'currPercentComplete': 0,
          'isComplete': false
        },
        'progressArcPoints': [],
        'backgroundArcPoints': checkPoints
      },
      // Verify that progress is drawn starting from the top of the circle
      // (3pi/2 rad) and moving clockwise.
      {
        'setProgressArgs': {
          'prevPercentComplete': 0,
          'currPercentComplete': 40,
          'isComplete': false
        },
        'progressArcPoints': [
          {x: 185, y: 40} /* 7pi/4 rad */, {x: 185, y: 110} /* pi/4 rad */
        ],
        'backgroundArcPoints': [
          {x: 115, y: 110} /* 3pi/4 rad */, {x: 115, y: 40} /* 5pi/4 rad */
        ]
      },
      // Verify that the progress drawn includes progress made previously.
      {
        'setProgressArgs': {
          'prevPercentComplete': 40,
          'currPercentComplete': 80,
          'isComplete': false
        },
        'progressArcPoints': [
          {x: 185, y: 40} /* 7pi/4 rad */, {x: 185, y: 110} /* pi/4 rad */,
          {x: 115, y: 110} /* 3pi/4 rad */
        ],
        'backgroundArcPoints': [
          {x: 115, y: 40} /* 5pi/4 rad */
        ]
      },
      // Verify that progress past 100% gets capped rather than wrapping around.
      {
        'setProgressArgs': {
          'prevPercentComplete': 80,
          'currPercentComplete': 160,
          'isComplete': false
        },
        'progressArcPoints': checkPoints,
        'backgroundArcPoints': []
      },
      // Verify that if the enrollment is complete, maximum progress is drawn.
      {
        'setProgressArgs': {
          'prevPercentComplete': 80,
          'currPercentComplete': 80,
          'isComplete': true
        },
        'progressArcPoints': checkPoints,
        'backgroundArcPoints': []
      }
    ];

    for (const {setProgressArgs, progressArcPoints, backgroundArcPoints} of
             testCases) {
      clearCanvas(canvas);
      assertListOfColorsEqual(canvasColor, checkPoints);

      progressArc.setProgress(
          setProgressArgs.prevPercentComplete,
          setProgressArgs.currPercentComplete, setProgressArgs.isComplete);
      await eventToPromise('cr-fingerprint-progress-arc-drawn', progressArc);

      assertListOfColorsEqual(circleProgressColor, progressArcPoints);
      assertListOfColorsEqual(circleBackgroundColor, backgroundArcPoints);
    }
  });

  test('TestSwitchToDarkMode', function() {
    const scanningAnimation = progressArc.$.scanningAnimation;
    progressArc.setProgress(0, 1, true);
    assertEquals(FINGERPRINT_TICK_LIGHT_URL, scanningAnimation.animationUrl);

    fakeMediaQueryList.matches = true;
    assertEquals(FINGERPRINT_TICK_DARK_URL, scanningAnimation.animationUrl);
  });
});
