// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-lottie. */
cr.define('cr_lottie_test', function() {
  /**
   * A data url that produces a sample json lottie animation.
   * @type {string}
   */
  const SAMPLE_LOTTIE = 'data:application/json;base64,eyJ2IjoiNC42LjkiLCJmci' +
      'I6NjAsImlwIjowLCJvcCI6MjAwLCJ3Ijo4MDAsImgiOjYwMCwiZGRkIjowLCJhc3NldHM' +
      'iOltdLCJsYXllcnMiOlt7ImluZCI6MSwidHkiOjEsInNjIjoiIzAwZmYwMCIsImFvIjow' +
      'LCJpcCI6MCwib3AiOjIwMCwic3QiOjAsInNyIjoxLCJzdyI6ODAwLCJzaCI6NjAwLCJib' +
      'SI6MCwia3MiOnsibyI6eyJhIjowLCJrIjoxMDB9LCJyIjp7ImEiOjAsImsiOlswLDAsMF' +
      '19LCJwIjp7ImEiOjAsImsiOlszMDAsMjAwLDBdfSwiYSI6eyJhIjowLCJrIjpbMzAwLDI' +
      'wMCwwXX0sInMiOnsiYSI6MCwiayI6WzEwMCwxMDAsMTAwXX19fV19';

  /**
   * A dataURL of an image for how a frame of the above |sampleLottie| animation
   * looks like.
   * @type {string}
   */
  const EXPECTED_FRAME = 'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD' +
      '/2wBDABALDA4MChAODQ4SERATGCgaGBYWGDEjJR0oOjM9PDkzODdASFxOQERXRTc4UG1R' +
      'V19iZ2hnPk1xeXBkeFxlZ2P/2wBDARESEhgVGC8aGi9jQjhCY2NjY2NjY2NjY2NjY2NjY' +
      '2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2P/wAARCADIASwDASIAAhEBAx' +
      'EB/8QAGAABAQEBAQAAAAAAAAAAAAAAAAMGBwX/xAAdEAEAAAYDAAAAAAAAAAAAAAAAAQI' +
      'DBDRzBrHB/8QAGAEBAAMBAAAAAAAAAAAAAAAAAAIEBgX/xAAbEQEAAgIDAAAAAAAAAAAA' +
      'AAAAAQIyMwQFgf/aAAwDAQACEQMRAD8A5+ADotlhW+uXpZGywrfXL0szNspY++UgCKIAA' +
      'AAAAAAAAAAAAAAAAAAAAAAAAAAyfL82hr9i1jJ8vzaGv2K5wt0L3Xb49eAA7jSAAOi2WF' +
      'b65elkbLCt9cvSzM2ylj75SAIogAAAAAAAAAAAAAAAAAAAAAAAAAAADJ8vzaGv2LWMny/' +
      'Noa/YrnC3Qvddvj14ADuNIAA6LZYVvrl6WRssK31y9LMzbKWPvlIAiiAAAAAAAAAAAAAA' +
      'AAAAAAAAAAAAAAMny/Noa/YtYyfL82hr9iucLdC912+PXgAO40gADotlhW+uXpZGywrfX' +
      'L0szNspY++UgCKIAAAAAAAAAAAAAAAAAAAAAAAAAAAAyfL82hr9i1jJ8vzaGv2K5wt0L3' +
      'Xb49eAA7jSAAOi2WFb65elkbLCt9cvSzM2ylj75SAIogAAAAAAAAAAAAAAAAAAAAAAAAA' +
      'AADJ8vzaGv2LWMny/Noa/YrnC3Qvddvj14ADuNIAA6LZYVvrl6WRssK31y9LMzbKWPvlI' +
      'AiiAAAAAAAAAAAAAAAAAAAAAAAAAAAAMny/Noa/YtYyfL82hr9iucLdC912+PXgAO40gA' +
      'DotlhW+uXpZGywrfXL0szNspY++UgCKIAAAAAAAAAAAAAAAAAAAAAAAAAAAAyfL82hr9i' +
      '1jJ8vzaGv2K5wt0L3Xb49eAA7jSAAOi2WFb65elkbLCt9cvSzM2ylj75SAIogAAAAAAAA' +
      'AAAAAAAAAAAAAAAAAAADJ8vzaGv2LWMny/Noa/YrnC3Qvddvj14ADuNIAA6LZYVvrl6WR' +
      'ssK31y9LMzbKWPvlIAiiAAAAAAAAAAAAAAAAAAAAAAAAAAAAMny/Noa/YtYyfL82hr9iu' +
      'cLdC912+PXgAO40gADotlhW+uXpZGywrfXL0szNspY++UgCKIAAAAAAAAAAAAAAAAAAAA' +
      'AAAAAAAAyfL82hr9i1jJ8vzaGv2K5wt0L3Xb49eAA7jSAAOi2WFb65elkbLCt9cvSzM2y' +
      'lj75SAIogAAAAAAAAAAAAAAAAAAAAAAAAAAADJ8vzaGv2LWMny/Noa/YrnC3Qvddvj14A' +
      'DuNIAA6LZYVvrl6WRssK31y9LMzbKWPvlIAiiAAAAAAAAAAAAAAAAAAAAAAAAAAAAMny/' +
      'Noa/YtYyfL82hr9iucLdC912+PXgAO40gADotlhW+uXpYGZtlLH3ykARRAAAAAAAAAAAA' +
      'AAAAAAAAAAAAAAAAGT5fm0NfsQXOFuhe67fHrwAHcaR//Z';

  /** @type {!CrLottieElement} */
  let crLottieElement;

  /** @type {!HTMLDivElement} */
  let container;

  /** @type {?HTMLCanvasElement} */
  let canvas = null;

  setup(function() {
    PolymerTest.clearBody();
    crLottieElement = document.createElement('cr-lottie');
    crLottieElement.animationUrl = SAMPLE_LOTTIE;
    crLottieElement.autoplay = true;

    container = document.createElement('div');
    container.style.width = '300px';
    container.style.height = '200px';
    document.body.appendChild(container);
    container.appendChild(crLottieElement);

    canvas = crLottieElement.offscreenCanvas_;

    Polymer.dom.flush();
  });

  test('TestInitializeAnimationAndAutoPlay', async () => {
    assertFalse(crLottieElement.isAnimationLoaded_);
    const waitForInitializeEvent =
        test_util.eventToPromise('cr-lottie-initialized', crLottieElement);
    await waitForInitializeEvent;
    assertTrue(crLottieElement.isAnimationLoaded_);

    const waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    await waitForPlayingEvent;
  });

  test('TestResize', async () => {
    const waitForInitializeEvent =
        test_util.eventToPromise('cr-lottie-initialized', crLottieElement);
    await waitForInitializeEvent;

    const waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    await waitForPlayingEvent;

    const newHeight = 300;
    const newWidth = 400;
    const waitForResizeEvent =
        test_util.eventToPromise('cr-lottie-resized', crLottieElement)
            .then(function(e) {
              assertEquals(e.detail.height, newHeight);
              assertEquals(e.detail.width, newWidth);
            });

    // Update size of parent div container to see if the canvas is resized.
    container.style.width = newWidth + 'px';
    container.style.height = newHeight + 'px';
    await waitForResizeEvent;
  });

  test('TestPlayPause', async () => {
    const waitForInitializeEvent =
        test_util.eventToPromise('cr-lottie-initialized', crLottieElement);
    await waitForInitializeEvent;

    const waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    await waitForPlayingEvent;

    const waitForPauseEvent =
        test_util.eventToPromise('cr-lottie-paused', crLottieElement);
    crLottieElement.setPlay(false);
    await waitForPauseEvent;

    const waitForPlayingEventAgain =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    crLottieElement.setPlay(true);
    await waitForPlayingEventAgain;
  });

  test('TestPlayBeforeInit', async () => {
    assertTrue(crLottieElement.autoplay);

    crLottieElement.setPlay(false);
    assertFalse(crLottieElement.autoplay);

    crLottieElement.setPlay(true);
    assertTrue(crLottieElement.autoplay);

    const waitForInitializeEvent =
        test_util.eventToPromise('cr-lottie-initialized', crLottieElement);
    await waitForInitializeEvent;

    const waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    await waitForPlayingEvent;
  });

  test('TestRenderFrame', async () => {
    // Offscreen canvas has a race issue when used in this test framework. To
    // ensure that we capture a frame from the animation and not an empty frame,
    // we delay the capture by 2 seconds.
    // Note: This issue is only observed in tests.
    const kRaceTimeout = 2000;

    const waitForInitializeEvent =
        test_util.eventToPromise('cr-lottie-initialized', crLottieElement);
    await waitForInitializeEvent;

    const waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    await waitForPlayingEvent;

    const waitForFrameRender = new Promise(function(resolve) {
                                 setTimeout(resolve, kRaceTimeout);
                               }).then(function() {
      const actualFrame =
          crLottieElement.canvasElement_.toDataURL('image/jpeg', 0.5);
      assertEquals(actualFrame, EXPECTED_FRAME);
    });

    await waitForFrameRender;
  });
});
