// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';

import {CrLottieElement} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController, MockMethod} from 'chrome://webui-test/mock_controller.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for cr-lottie. */
suite('cr_lottie_test', function() {
  /**
   * A data url that produces a sample solid green json lottie animation.
   */
  const SAMPLE_LOTTIE_GREEN: string =
      'data:application/json;base64,eyJ2IjoiNC42LjkiLCJmciI6NjAsImlwIjowLCJvc' +
      'CI6MjAwLCJ3Ijo4MDAsImgiOjYwMCwiZGRkIjowLCJhc3NldHMiOltdLCJsYXllcnMiOlt' +
      '7ImluZCI6MSwidHkiOjEsInNjIjoiIzAwZmYwMCIsImFvIjowLCJpcCI6MCwib3AiOjIwM' +
      'Cwic3QiOjAsInNyIjoxLCJzdyI6ODAwLCJzaCI6NjAwLCJibSI6MCwia3MiOnsibyI6eyJ' +
      'hIjowLCJrIjoxMDB9LCJyIjp7ImEiOjAsImsiOlswLDAsMF19LCJwIjp7ImEiOjAsImsiO' +
      'lszMDAsMjAwLDBdfSwiYSI6eyJhIjowLCJrIjpbMzAwLDIwMCwwXX0sInMiOnsiYSI6MCw' +
      'iayI6WzEwMCwxMDAsMTAwXX19fV19';

  /**
   * A data url that produces a sample solid blue json lottie animation.
   */
  const SAMPLE_LOTTIE_BLUE: string =
      'data:application/json;base64,eyJhc3NldHMiOltdLCJkZGQiOjAsImZyIjo2MCwia' +
      'CI6NjAwLCJpcCI6MCwibGF5ZXJzIjpbeyJhbyI6MCwiYm0iOjAsImluZCI6MSwiaXAiOjA' +
      'sImtzIjp7ImEiOnsiYSI6MCwiayI6WzMwMCwyMDAsMF19LCJvIjp7ImEiOjAsImsiOjEwM' +
      'H0sInAiOnsiYSI6MCwiayI6WzMwMCwyMDAsMF19LCJyIjp7ImEiOjAsImsiOlswLDAsMF1' +
      '9LCJzIjp7ImEiOjAsImsiOlsxMDAsMTAwLDEwMF19fSwib3AiOjIwMCwic2MiOiIjMDAwM' +
      'GZmIiwic2giOjYwMCwic3IiOjEsInN0IjowLCJzdyI6ODAwLCJ0eSI6MX1dLCJvcCI6MjA' +
      'wLCJ2IjoiNC42LjkiLCJ3Ijo4MDB9';

  /**
   * A green pixel as returned by samplePixel.
   */
  const GREEN_PIXEL: number[] = [0, 255, 0, 255];

  /**
   * A blue pixel as returned by samplePixel.
   */
  const BLUE_PIXEL: number[] = [0, 0, 255, 255];

  let mockController: MockController;
  let crLottieElement: CrLottieElement;

  let container: HTMLElement;
  let canvas: HTMLCanvasElement;

  let waitForInitializeEvent: Promise<void>;
  let waitForPlayingEvent: Promise<void>;

  const defaultWidth = 300;
  const defaultHeight = 200;

  setup(function() {
    mockController = new MockController();
  });

  teardown(function() {
    mockController.reset();
  });

  function createLottieElement(autoplay: boolean = true) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crLottieElement = document.createElement('cr-lottie');
    crLottieElement.animationUrl = SAMPLE_LOTTIE_GREEN;
    crLottieElement.autoplay = autoplay;

    waitForInitializeEvent =
        eventToPromise('cr-lottie-initialized', crLottieElement);
    waitForPlayingEvent = eventToPromise('cr-lottie-playing', crLottieElement);

    container = document.createElement('div');
    container.style.width = defaultWidth + 'px';
    container.style.height = defaultHeight + 'px';
    document.body.appendChild(container);
    container.appendChild(crLottieElement);

    canvas = crLottieElement.$.canvas;

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
    const canvas = crLottieElement.$.canvas;
    img.setAttribute('src', canvas.toDataURL());
    await waitForLoad;

    const imgCanvas = document.createElement('canvas');
    imgCanvas.width = canvas.width;
    imgCanvas.height = canvas.height;

    const context = imgCanvas.getContext('2d')!;
    context.drawImage(img, 0, 0);

    return Array.from(
        context.getImageData(canvas.width / 2, canvas.height / 2, 1, 1).data);
  }

  test('TestResize', async () => {
    createLottieElement(/*autoplay=*/ true);
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    // First resize event after loading the animation.
    const firstResizeEventAfterLoad =
        eventToPromise('cr-lottie-resized', crLottieElement);
    const firstResizeEvent = await firstResizeEventAfterLoad;
    assertEquals(firstResizeEvent.detail.height, defaultHeight);
    assertEquals(firstResizeEvent.detail.width, defaultWidth);

    // Update size of parent div container to see if the canvas is resized.
    const newHeight = 300;
    const newWidth = 400;
    container.style.width = newWidth + 'px';
    container.style.height = newHeight + 'px';
    const resizeEventAfterExplicitResize =
        eventToPromise('cr-lottie-resized', crLottieElement);
    const resizeEvent = await resizeEventAfterExplicitResize;

    assertEquals(resizeEvent.detail.height, newHeight);
    assertEquals(resizeEvent.detail.width, newWidth);
  });


  test('TestResizeBeforeInit', async () => {
    // Tests that resize events are properly handled, even if they happen
    // before the initialization is done.
    createLottieElement(/*autoplay=*/ true);

    // Resize while initialization is going on.
    const newHeight = 300;
    const newWidth = 400;
    const waitForResizeEvent =
        eventToPromise('cr-lottie-resized', crLottieElement);
    // Update size of parent div container to see if the canvas is resized.
    container.style.width = newWidth + 'px';
    container.style.height = newHeight + 'px';

    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const resizeEvent = await waitForResizeEvent;
    assertEquals(resizeEvent.detail.height, newHeight);
    assertEquals(resizeEvent.detail.width, newWidth);
  });

  test('TestPlayPause', async () => {
    createLottieElement(/*autoplay=*/ true);
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

  test('TestAutoplayWorksWhenTrue', async () => {
    createLottieElement(/*autoplay=*/ true);
    assertTrue(crLottieElement.autoplay);
    await waitForInitializeEvent;
    await waitForPlayingEvent;
  });

  test('TestAutoplayWorksWhenFalse', async () => {
    createLottieElement(/*autoplay=*/ false);
    assertFalse(crLottieElement.autoplay);

    // Since setting 'autoplay' to false will cause the play event to never be
    // fired, we use Promise.race to check if it resolves in 2 seconds.
    const playTimeoutNotice = 'PLAY_TIMEOUT';
    const playEventTimeout = new Promise((resolve) => {
      setTimeout(() => resolve(playTimeoutNotice), 2000);
    });

    await waitForInitializeEvent;
    const playEventResult =
        await Promise.race([playEventTimeout, waitForPlayingEvent]);
    assertEquals(playEventResult, playTimeoutNotice);
  });

  test('TestPlayBeforeInit', async () => {
    // Tests that a play request sent during initialization will be fulfilled.
    createLottieElement(/*autoplay=*/ false);
    assertFalse(crLottieElement.autoplay);
    crLottieElement.setPlay(true);

    await waitForInitializeEvent;
    await waitForPlayingEvent;
  });

  test('TestPauseBeforeInit', async () => {
    // Tests that a pause request sent during initialization will be fulfilled.
    createLottieElement(/*autoplay=*/ true);
    assertTrue(crLottieElement.autoplay);

    crLottieElement.setPlay(false);
    const waitForPauseEvent =
        eventToPromise('cr-lottie-paused', crLottieElement);
    await waitForInitializeEvent;
    await waitForPauseEvent;
  });

  test('TestRenderFrame', async function() {
    // TODO(crbug.com/1108915): Offscreen canvas has a race issue when used in
    // this test framework.
    if (1) {
      this.skip();
    }

    // To ensure that we capture a frame from the animation and not an empty
    // frame, we delay the capture by 2 seconds.
    // Note: This issue is only observed in tests.
    const kRaceTimeout = 2000;

    createLottieElement(/*autoplay=*/ true);
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForFrameRender = new Promise<void>(function(resolve) {
      window.setTimeout(resolve, kRaceTimeout);
    });

    await waitForFrameRender;

    assertDeepEquals(GREEN_PIXEL, await samplePixel());
  });

  test('TestChangeAnimationUrl', async function() {
    // TODO(crbug.com/1108915): Offscreen canvas has a race issue when used in
    // this test framework.
    if (1) {
      this.skip();
    }

    // To ensure that we capture a frame from the animation and not an empty
    // frame, we delay the capture by 2 seconds.
    // Note: This issue is only observed in tests.
    const kRaceTimeout = 2000;

    createLottieElement(/*autoplay=*/ true);
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

    const waitForFrameRender = new Promise<void>(function(resolve) {
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
    } as unknown as XMLHttpRequest;

    mockXhr.open = mockController.createFunctionMock(mockXhr, 'open') as any;
    mockXhr.send = mockController.createFunctionMock(mockXhr, 'send') as any;
    mockXhr.abort = mockController.createFunctionMock(mockXhr, 'abort') as any;

    const mockXhrConstructor =
        mockController.createFunctionMock(window, 'XMLHttpRequest');

    // Expectations for loading the image and aborting it.
    mockXhrConstructor.addExpectation();
    (mockXhr.open as unknown as MockMethod)
        .addExpectation('GET', SAMPLE_LOTTIE_GREEN, true);
    (mockXhr.send as unknown as MockMethod).addExpectation();
    (mockXhr.abort as unknown as MockMethod).addExpectation();

    mockXhrConstructor.returnValue = mockXhr;

    createLottieElement(/*autoplay=*/ true);

    // Detaching the element before the image has loaded should abort the
    // request.
    crLottieElement.remove();
    mockController.verifyMocks();
  });

  test('TestLoadNewImageWhileOldImageIsStillLoading', async () => {
    const mockXhr = {
      onreadystatechange: () => {},
    } as unknown as XMLHttpRequest;

    mockXhr.open = mockController.createFunctionMock(mockXhr, 'open') as any;
    mockXhr.send = mockController.createFunctionMock(mockXhr, 'send') as any;
    mockXhr.abort = mockController.createFunctionMock(mockXhr, 'abort') as any;

    const mockXhrConstructor =
        mockController.createFunctionMock(window, 'XMLHttpRequest');

    // Expectations for loading the first image and aborting it.
    mockXhrConstructor.addExpectation();
    (mockXhr.open as unknown as MockMethod)
        .addExpectation('GET', SAMPLE_LOTTIE_GREEN, true);
    (mockXhr.send as unknown as MockMethod).addExpectation();
    (mockXhr.abort as unknown as MockMethod).addExpectation();

    // Expectations for loading the second image.
    mockXhrConstructor.addExpectation();
    (mockXhr.open as unknown as MockMethod)
        .addExpectation('GET', SAMPLE_LOTTIE_BLUE, true);
    (mockXhr.send as unknown as MockMethod).addExpectation();

    mockXhrConstructor.returnValue = mockXhr;

    createLottieElement(/*autoplay=*/ true);

    // Attempting to load a new image should abort the first request and start a
    // new one.
    crLottieElement.animationUrl = SAMPLE_LOTTIE_BLUE;

    mockController.verifyMocks();
  });
});
