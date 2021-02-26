// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {LOTTIE_JS_URL} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {MockController, MockMethod} from '../mock_controller.m.js';
import {eventToPromise} from '../test_util.m.js';
// clang-format on

/** @fileoverview Suite of tests for cr-lottie. */
suite('cr_lottie_test', function() {
  /**
   * A data url that produces a sample solid green json lottie animation.
   * @type {string}
   */
  const SAMPLE_LOTTIE_GREEN =
      'data:application/json;base64,eyJ2IjoiNC42LjkiLCJmciI6NjAsImlwIjowLCJvc' +
      'CI6MjAwLCJ3Ijo4MDAsImgiOjYwMCwiZGRkIjowLCJhc3NldHMiOltdLCJsYXllcnMiOlt' +
      '7ImluZCI6MSwidHkiOjEsInNjIjoiIzAwZmYwMCIsImFvIjowLCJpcCI6MCwib3AiOjIwM' +
      'Cwic3QiOjAsInNyIjoxLCJzdyI6ODAwLCJzaCI6NjAwLCJibSI6MCwia3MiOnsibyI6eyJ' +
      'hIjowLCJrIjoxMDB9LCJyIjp7ImEiOjAsImsiOlswLDAsMF19LCJwIjp7ImEiOjAsImsiO' +
      'lszMDAsMjAwLDBdfSwiYSI6eyJhIjowLCJrIjpbMzAwLDIwMCwwXX0sInMiOnsiYSI6MCw' +
      'iayI6WzEwMCwxMDAsMTAwXX19fV19';

  /**
   * A data url that produces a sample solid blue json lottie animation.
   * @type {string}
   */
  const SAMPLE_LOTTIE_BLUE =
      'data:application/json;base64,eyJhc3NldHMiOltdLCJkZGQiOjAsImZyIjo2MCwia' +
      'CI6NjAwLCJpcCI6MCwibGF5ZXJzIjpbeyJhbyI6MCwiYm0iOjAsImluZCI6MSwiaXAiOjA' +
      'sImtzIjp7ImEiOnsiYSI6MCwiayI6WzMwMCwyMDAsMF19LCJvIjp7ImEiOjAsImsiOjEwM' +
      'H0sInAiOnsiYSI6MCwiayI6WzMwMCwyMDAsMF19LCJyIjp7ImEiOjAsImsiOlswLDAsMF1' +
      '9LCJzIjp7ImEiOjAsImsiOlsxMDAsMTAwLDEwMF19fSwib3AiOjIwMCwic2MiOiIjMDAwM' +
      'GZmIiwic2giOjYwMCwic3IiOjEsInN0IjowLCJzdyI6ODAwLCJ0eSI6MX1dLCJvcCI6MjA' +
      'wLCJ2IjoiNC42LjkiLCJ3Ijo4MDB9';

  /**
   * A green pixel as returned by samplePixel.
   * @type {!Array<number>}
   */
  const GREEN_PIXEL = [0, 255, 0, 255];

  /**
   * A blue pixel as returned by samplePixel.
   * @type {!Array<number>}
   */
  const BLUE_PIXEL = [0, 0, 255, 255];

  /** @type {?MockController} */
  let mockController;

  /** @type {!CrLottieElement} */
  let crLottieElement;

  /** @type {!HTMLDivElement} */
  let container;

  /** @type {?HTMLCanvasElement} */
  let canvas = null;

  /** @type {?Blob} */
  let lottieWorkerJs = null;

  /** @type {Promise} */
  let waitForInitializeEvent;

  /** @type {Promise} */
  let waitForPlayingEvent;

  setup(function(done) {
    mockController = new MockController();

    const xhr = new XMLHttpRequest();
    xhr.open('GET', LOTTIE_JS_URL, true);
    xhr.responseType = 'blob';
    xhr.send();
    xhr.onreadystatechange = function() {
      if (xhr.readyState === 4) {
        assertEquals(200, xhr.status);
        lottieWorkerJs = /** @type {Blob} */ (xhr.response);
        done();
      }
    };
  });

  teardown(function() {
    mockController.reset();
  });

  function createLottieElement() {
    document.body.innerHTML = '';
    crLottieElement =
        /** @type {!CrLottieElement} */ (document.createElement('cr-lottie'));
    crLottieElement.animationUrl = SAMPLE_LOTTIE_GREEN;
    crLottieElement.autoplay = true;

    waitForInitializeEvent =
        eventToPromise('cr-lottie-initialized', crLottieElement);
    waitForPlayingEvent = eventToPromise('cr-lottie-playing', crLottieElement);

    container = /** @type {!HTMLDivElement} */ (document.createElement('div'));
    container.style.width = '300px';
    container.style.height = '200px';
    document.body.appendChild(container);
    container.appendChild(crLottieElement);

    canvas = /** @type {!HTMLCanvasElement} */ (crLottieElement.$$('canvas'));

    flush();
  }

  /**
   * Samples a pixel from the lottie canvas.
   *
   * @return {!Promise<!Array<number>>} the pixel color as a [red, green, blue,
   * transparency] tuple with values 0-255.
   */
  async function samplePixel() {
    // It's not possible to get the context from a canvas that had its control
    // transferred to an OffscreenCanvas, or from a detached OffscreenCanvas.
    // Instead, copy the rendered canvas into a new canvas and sample a pixel
    // from it.
    const img = document.createElement('img');
    const waitForLoad = eventToPromise('load', img);
    const canvas = crLottieElement.$$('canvas');
    img.setAttribute('src', canvas.toDataURL());
    await waitForLoad;

    const imgCanvas = document.createElement('canvas');
    imgCanvas.width = canvas.width;
    imgCanvas.height = canvas.height;

    const context = imgCanvas.getContext('2d');
    context.drawImage(img, 0, 0);

    return Array.from(
        context.getImageData(canvas.width / 2, canvas.height / 2, 1, 1).data);
  }

  test('TestResize', async () => {
    createLottieElement();
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const newHeight = 300;
    const newWidth = 400;
    const waitForResizeEvent =
        /** @type {!Promise<!CustomEvent<{width: number, height: number}>>} */ (
            eventToPromise('cr-lottie-resized', crLottieElement));

    // Update size of parent div container to see if the canvas is resized.
    container.style.width = newWidth + 'px';
    container.style.height = newHeight + 'px';
    const resizeEvent = await waitForResizeEvent;

    assertEquals(resizeEvent.detail.height, newHeight);
    assertEquals(resizeEvent.detail.width, newWidth);
  });

  test('TestPlayPause', async () => {
    createLottieElement();
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForPauseEvent =
        eventToPromise('cr-lottie-paused', crLottieElement);
    crLottieElement.setPlay(false);
    await waitForPauseEvent;

    waitForPlayingEvent = eventToPromise('cr-lottie-playing', crLottieElement);
    crLottieElement.setPlay(true);
    await waitForPlayingEvent;
  });

  test('TestPlayBeforeInit', async () => {
    createLottieElement();
    assertTrue(crLottieElement.autoplay);

    crLottieElement.setPlay(false);
    assertFalse(crLottieElement.autoplay);

    crLottieElement.setPlay(true);
    assertTrue(crLottieElement.autoplay);

    await waitForInitializeEvent;
    await waitForPlayingEvent;
  });

  test('TestRenderFrame', async () => {
    // TODO(crbug.com/1108915): Offscreen canvas has a race issue when used in
    // this test framework. To ensure that we capture a frame from the animation
    // and not an empty frame, we delay the capture by 2 seconds.
    // Note: This issue is only observed in tests.
    const kRaceTimeout = 2000;

    createLottieElement();
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForFrameRender = new Promise(function(resolve) {
      window.setTimeout(resolve, kRaceTimeout);
    });

    await waitForFrameRender;

    assertDeepEquals(GREEN_PIXEL, await samplePixel());
  });

  test('TestChangeAnimationUrl', async () => {
    // TODO(crbug.com/1108915): Offscreen canvas has a race issue when used in
    // this test framework. To ensure that we capture a frame from the animation
    // and not an empty frame, we delay the capture by 2 seconds.
    // Note: This issue is only observed in tests.
    const kRaceTimeout = 2000;

    createLottieElement();
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForStoppedEvent =
        eventToPromise('cr-lottie-stopped', crLottieElement);
    waitForInitializeEvent =
        eventToPromise('cr-lottie-initialized', crLottieElement);
    waitForPlayingEvent = eventToPromise('cr-lottie-playing', crLottieElement);

    crLottieElement.animationUrl = SAMPLE_LOTTIE_BLUE;

    // The previous animation should be cleared and stopped between loading.
    // Unfortunately since the offscreen canvas is rendered asynchronously,
    // there is no way to grab a frame in between events and have it guaranteed
    // to be the empty frame. At least wait for the `cr-lottie-stopped` event.
    await waitForStoppedEvent;

    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForFrameRender = new Promise(function(resolve) {
      setTimeout(resolve, kRaceTimeout);
    });

    await waitForFrameRender;

    assertDeepEquals(BLUE_PIXEL, await samplePixel());
  });

  test('TestHidden', async () => {
    await waitForPlayingEvent;

    assertFalse(canvas.hidden);
    crLottieElement.hidden = true;
    assertTrue(canvas.hidden);
  });

  test('TestDetachBeforeImageLoaded', async () => {
    const mockXhr = {
      onreadystatechange: () => {},
    };
    mockXhr.open = mockController.createFunctionMock(mockXhr, 'open');
    mockXhr.send = mockController.createFunctionMock(mockXhr, 'send');
    mockXhr.abort = mockController.createFunctionMock(mockXhr, 'abort');

    const mockXhrConstructor =
        mockController.createFunctionMock(window, 'XMLHttpRequest');

    // Expectations for loading the worker.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation(
        'GET', 'chrome://resources/lottie/lottie_worker.min.js', true);
    mockXhr.send.addExpectation();

    // Expectations for loading the image and aborting it.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation('GET', SAMPLE_LOTTIE_GREEN, true);
    mockXhr.send.addExpectation();
    mockXhr.abort.addExpectation();

    mockXhrConstructor.returnValue = mockXhr;

    createLottieElement();

    // Return the lottie worker.
    mockXhr.response = lottieWorkerJs;
    mockXhr.readyState = 4;
    mockXhr.status = 200;
    mockXhr.onreadystatechange();

    // Detaching the element before the image has loaded should abort the
    // request.
    crLottieElement.remove();
    mockController.verifyMocks();
  });

  test('TestLoadNewImageWhileOldImageIsStillLoading', async () => {
    const mockXhr = {
      onreadystatechange: () => {},
    };
    mockXhr.open = mockController.createFunctionMock(mockXhr, 'open');
    mockXhr.send = mockController.createFunctionMock(mockXhr, 'send');
    mockXhr.abort = mockController.createFunctionMock(mockXhr, 'abort');

    const mockXhrConstructor =
        mockController.createFunctionMock(window, 'XMLHttpRequest');

    // Expectations for loading the worker.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation(
        'GET', 'chrome://resources/lottie/lottie_worker.min.js', true);
    mockXhr.send.addExpectation();

    // Expectations for loading the first image and aborting it.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation('GET', SAMPLE_LOTTIE_GREEN, true);
    mockXhr.send.addExpectation();
    mockXhr.abort.addExpectation();

    // Expectations for loading the second image.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation('GET', SAMPLE_LOTTIE_BLUE, true);
    mockXhr.send.addExpectation();

    mockXhrConstructor.returnValue = mockXhr;

    createLottieElement();

    // Return the lottie worker.
    mockXhr.response = lottieWorkerJs;
    mockXhr.readyState = 4;
    mockXhr.status = 200;
    mockXhr.onreadystatechange();

    // Attempting to load a new image should abort the first request and start a
    // new one.
    crLottieElement.animationUrl = SAMPLE_LOTTIE_BLUE;

    mockController.verifyMocks();
  });
});
