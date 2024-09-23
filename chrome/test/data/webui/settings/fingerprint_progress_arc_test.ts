// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {FingerprintProgressArcElement} from 'chrome://settings/lazy_load.js';
import {FINGERPRINT_CHECK_DARK_URL, FINGERPRINT_CHECK_LIGHT_URL, FINGERPRINT_SCANNED_ICON_DARK, FINGERPRINT_SCANNED_ICON_LIGHT, PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK, PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT, PROGRESS_CIRCLE_FILL_COLOR_DARK, PROGRESS_CIRCLE_FILL_COLOR_LIGHT} from 'chrome://settings/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

/** @fileoverview Suite of tests for fingerprint-progress-arc. */
suite('cr_fingerprint_progress_arc_test', function() {
  /**
   * An object descrbing a 2d point.
   */
  interface Point {
    x: number;
    y: number;
  }

  const canvasColor: string = 'rgba(255, 255, 255, 1.0)';

  let progressArc: FingerprintProgressArcElement;
  let canvas: HTMLCanvasElement;
  let mockController: MockController;
  let fakeMediaQueryList: FakeMediaQueryList;

  function clearCanvas(canvas: HTMLCanvasElement) {
    const ctx = canvas.getContext('2d')!;
    ctx.fillStyle = canvasColor;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }

  setup(function() {
    mockController = new MockController();
    const matchMediaMock =
        mockController.createFunctionMock(window, 'matchMedia');
    fakeMediaQueryList = new FakeMediaQueryList('(prefers-color-scheme: dark)');
    matchMediaMock.returnValue = fakeMediaQueryList;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    progressArc = document.createElement('fingerprint-progress-arc');
    document.body.appendChild(progressArc);

    // Override some parameters and function for testing purposes.
    canvas = progressArc.$.canvas;
    canvas.width = 300;
    canvas.height = 150;
    progressArc.circleRadius = 50;
    clearCanvas(progressArc.$.canvas);
    flush();
  });

  teardown(function() {
    mockController.reset();
  });

  /**
   * Helper function which gets the (r, g, b, a)-formatted color at |point| on
   * the canvas.
   */
  function getColor(point: Point): string {
    const ctx = canvas.getContext('2d')!;
    const pixel = ctx.getImageData(point.x, point.y, 1, 1).data;
    return `rgba(${pixel[0]!}, ${pixel[1]!}, ${pixel[2]!}, ${
        (pixel[3]! / 255).toFixed(1)})`;
  }

  /**
   * Helper function which checks that |expectedColor| matches the canvas's
   * color at each point in |listOfPoints|.
   */
  function assertListOfColorsEqual(
      expectedColor: string, listOfPoints: Point[]) {
    for (const point of listOfPoints) {
      assertEquals(expectedColor, getColor(point));
    }
  }

  /**
   * Helper class that, given a set of arguments for a |setProgress()| function
   * call, can be used to perform that function call and verify that the
   * resulting progress circle is drawn correctly.
   */
  class SetProgressTestCase {
    // Angles on HTML canvases start at 0 radians on the positive x-axis and
    // increase in the clockwise direction. Progress is drawn starting from the
    // top of the circle (3pi/2 rad). In the verification step, a test case
    // checks the colors drawn at angles 7pi/4, pi/4, 3pi/4, and 5pi/4 rad
    // (respectively 12.5%, 37.5%, 62.5%, and 87.5% progress completed).
    static progressCheckPoints: Array<[Point, number]> = [
      [{x: 185, y: 40} /**  7pi/4 rad */, 12.5],
      [{x: 185, y: 110} /**  pi/4 rad */, 37.5],
      [{x: 115, y: 110} /** 3pi/4 rad */, 62.5],
      [{x: 115, y: 40} /**  5pi/4 rad */, 87.5],
    ];

    // |setProgress()| arguments.
    private prevPercentComplete_: number;
    private currPercentComplete_: number;
    private isComplete_: boolean;

    // Points expected to be part of the circle's progress arc.
    private progressPoints_: Point[] = [];

    // Points expected to be part of the circle's background arc.
    private backgroundPoints_: Point[] = [];

    constructor(
        prevPercentComplete: number, currPercentComplete: number,
        isComplete: boolean) {
      this.prevPercentComplete_ = prevPercentComplete;
      this.currPercentComplete_ = currPercentComplete;
      this.isComplete_ = isComplete;

      const endPercent = isComplete ? 100 : Math.min(100, currPercentComplete);
      for (const [point, percent] of SetProgressTestCase.progressCheckPoints) {
        if (percent <= endPercent) {
          this.progressPoints_.push(point);
        } else {
          this.backgroundPoints_.push(point);
        }
      }
    }

    // Draws a progress circle on a fresh canvas using the test case's
    // |setProgress()| arguments and verifies that the circle's colors match the
    // expected colors.
    async run() {
      clearCanvas(canvas);
      assertListOfColorsEqual(
          canvasColor, this.progressPoints_.concat(this.backgroundPoints_));

      progressArc.setProgress(
          this.prevPercentComplete_, this.currPercentComplete_,
          this.isComplete_);
      await eventToPromise('fingerprint-progress-arc-drawn', progressArc);

      this.verifyProgressCircleColors();
    }

    // Helper function used to compare the current progress circle colors
    // against the test case's expected colors. Assumes that drawing has
    // already been done.
    verifyProgressCircleColors() {
      const isDarkMode = fakeMediaQueryList.matches;
      assertListOfColorsEqual(
          isDarkMode ? PROGRESS_CIRCLE_FILL_COLOR_DARK :
                       PROGRESS_CIRCLE_FILL_COLOR_LIGHT,
          this.progressPoints_);
      assertListOfColorsEqual(
          isDarkMode ? PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK :
                       PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT,
          this.backgroundPoints_);
    }
  }

  /**
   * Test setting progress while in light mode, switching to dark mode,
   * continuing to set progress, switching back to light mode, and setting
   * progress again.
   */
  test('TestSetProgress', async () => {
    const lightModeTestCases1 = [
      // Verify that no progress is drawn when no progress has been made.
      new SetProgressTestCase(0, 0, false),
      // Verify that progress is drawn starting from the top of the circle
      // (3pi/2 rad) and moving clockwise.
      new SetProgressTestCase(0, 40, false),
    ];

    const darkModeTestCases = [
      // Verify that the progress drawn includes progress made previously.
      new SetProgressTestCase(40, 80, false),
      // Verify that progress past 100% gets capped rather than wrapping around.
      new SetProgressTestCase(80, 160, false),
    ];

    // Verify that if the enrollment is complete, maximum progress is drawn.
    const lightModeTestCases2 = [new SetProgressTestCase(80, 80, true)];

    // Make some progress in light mode.
    for (const testCase of lightModeTestCases1) {
      await testCase.run();
    }

    // Switch to dark mode and verify that the progress circle is redrawn.
    fakeMediaQueryList.matches = true;
    lightModeTestCases1.slice(-1)[0]!.verifyProgressCircleColors();

    // Make some progress in dark mode.
    for (const testCase of darkModeTestCases) {
      await testCase.run();
    }

    // Switch back to light mode and verify that the progress circle is redrawn.
    fakeMediaQueryList.matches = false;
    darkModeTestCases.slice(-1)[0]!.verifyProgressCircleColors();

    // Finish making progress in light mode.
    for (const testCase of lightModeTestCases2) {
      await testCase.run();
    }
  });

  test('TestSwitchToDarkMode', function() {
    const fingerprintScanned = progressArc.$.fingerprintScanned;
    const scanningAnimation = progressArc.$.scanningAnimation;

    progressArc.setProgress(0, 1, true);
    assertEquals(FINGERPRINT_SCANNED_ICON_LIGHT, fingerprintScanned.icon);
    assertEquals(FINGERPRINT_CHECK_LIGHT_URL, scanningAnimation.animationUrl);

    fakeMediaQueryList.matches = true;
    assertEquals(FINGERPRINT_SCANNED_ICON_DARK, fingerprintScanned.icon);
    assertEquals(FINGERPRINT_CHECK_DARK_URL, scanningAnimation.animationUrl);
  });
});
