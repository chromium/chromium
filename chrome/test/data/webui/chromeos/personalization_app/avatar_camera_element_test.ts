// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {AvatarCameraElement, AvatarCameraMode, GetUserMediaProxy, setWebcamUtilsForTesting} from 'chrome://personalization/js/personalization_app.js';
import * as webcamUtils from 'chrome://resources/ash/common/cr_picture/webcam_utils.js';
import {assertDeepEquals, assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestUserProvider} from './test_user_interface_provider';

type WebcamUtilsInterface = typeof webcamUtils;

class MockWebcamUtils extends TestBrowserProxy implements WebcamUtilsInterface {
  captureFramesResponse = [];
  pngUint8Array = new Uint8Array(10);

  /* eslint-disable @typescript-eslint/naming-convention */
  CAPTURE_SIZE = {height: 10, width: 10};
  CAPTURE_INTERVAL_MS = 10;
  CAPTURE_DURATION_MS = 20;
  /* eslint-enable @typescript-eslint/naming-convention */

  kDefaultVideoConstraints = webcamUtils.kDefaultVideoConstraints;

  constructor() {
    super(['captureFrames', 'stopMediaTracks', 'convertFramesToPngBinary']);
    this.pngUint8Array.fill(17);
  }

  convertFramesToPngBinary(frames: HTMLCanvasElement[]): Uint8Array {
    this.methodCalled('convertFramesToPngBinary', frames);
    return this.pngUint8Array;
  }

  convertFramesToPng(_: HTMLCanvasElement[]): string {
    assertNotReached('This function should never be called');
  }

  async captureFrames(
      video: HTMLVideoElement, captureSize: typeof webcamUtils.CAPTURE_SIZE,
      intervalMs: number, numFrames: number): Promise<HTMLCanvasElement[]> {
    this.methodCalled(
        'captureFrames', video, captureSize, intervalMs, numFrames);
    return Promise.resolve(this.captureFramesResponse);
  }

  stopMediaTracks(stream: MediaStream|null): void {
    this.methodCalled('stopMediaTracks', stream);
  }
}

class MockGetUserMediaProxy extends TestBrowserProxy implements
    GetUserMediaProxy {
  mediaStream = new MediaStream();

  constructor() {
    super(['getUserMedia']);
  }

  getUserMedia(): Promise<MediaStream> {
    this.methodCalled('getUserMedia');
    return Promise.resolve(this.mediaStream);
  }
}

suite('AvatarCameraElementTest', function() {
  let avatarCameraElement: AvatarCameraElement|null = null;
  let mockGetUserMediaProxy: MockGetUserMediaProxy;
  let mockWebcamUtils: MockWebcamUtils;
  let userProvider: TestUserProvider;


  setup(function() {
    mockWebcamUtils = new MockWebcamUtils();
    setWebcamUtilsForTesting(mockWebcamUtils);
    mockGetUserMediaProxy = new MockGetUserMediaProxy();
    GetUserMediaProxy.setInstanceForTesting(mockGetUserMediaProxy);
    const mocks = baseSetup();
    userProvider = mocks.userProvider;
  });

  teardown(async () => {
    await teardownElement(avatarCameraElement);
    avatarCameraElement = null;
  });

  test('requests webcam media when open and attaches to video', async () => {
    avatarCameraElement =
        initElement(AvatarCameraElement, {mode: AvatarCameraMode.CAMERA});
    await mockGetUserMediaProxy.whenCalled('getUserMedia');
    const video = avatarCameraElement.shadowRoot!.getElementById(
                      'webcamVideo') as HTMLVideoElement;
    assertEquals(
        mockGetUserMediaProxy.mediaStream, video.srcObject,
        'video.srcObject should equal media stream object');
  });

  test('shows preview confirm/cancel ui after takePhoto click', async () => {
    avatarCameraElement =
        initElement(AvatarCameraElement, {mode: AvatarCameraMode.CAMERA});
    await waitAfterNextRender(avatarCameraElement);

    const previewButtonIds = ['confirmPhoto', 'clearPhoto'];

    for (const buttonId of previewButtonIds) {
      assertEquals(
          null, avatarCameraElement.shadowRoot?.getElementById(buttonId),
          `${buttonId} button should not exist before photo is taken`);
    }

    const videoElement =
        avatarCameraElement.shadowRoot?.getElementById('webcamVideo');
    assertTrue(
        !!videoElement && !videoElement.hidden,
        'video element should be visible before takePhoto click');

    const takePhotoButton =
        avatarCameraElement.shadowRoot?.getElementById('takePhoto');
    assertTrue(!!takePhotoButton, 'take photo button should be visible');
    takePhotoButton.click();

    await mockWebcamUtils.whenCalled('captureFrames');
    await waitAfterNextRender(avatarCameraElement);

    for (const buttonId of previewButtonIds) {
      assertTrue(
          !!avatarCameraElement.shadowRoot?.getElementById(buttonId),
          `${buttonId} button should exist after photo is taken`);
    }

    assertTrue(
        videoElement.hidden, 'video element should be hidden during preview');
  });

  test('calls captureFrames on takePhoto click', async () => {
    avatarCameraElement =
        initElement(AvatarCameraElement, {mode: AvatarCameraMode.CAMERA});
    await waitAfterNextRender(avatarCameraElement);

    avatarCameraElement.shadowRoot?.getElementById('takePhoto')?.click();

    let [video, size, interval, numFrames] =
        await mockWebcamUtils.whenCalled('captureFrames');

    assertEquals(
        avatarCameraElement.shadowRoot?.getElementById('webcamVideo'), video,
        'Video element sent to captureFrames');

    assertDeepEquals({height: 10, width: 10}, size, 'Mock size used');
    assertEquals(10, interval, 'Mock interval value used');
    assertEquals(1, numFrames, 'Single frame requested for photo');

    avatarCameraElement.mode = AvatarCameraMode.VIDEO;
    await waitAfterNextRender(avatarCameraElement);

    mockWebcamUtils.resetResolver('captureFrames');
    avatarCameraElement.shadowRoot?.getElementById('takePhoto')?.click();

    [video, size, interval, numFrames] =
        await mockWebcamUtils.whenCalled('captureFrames');

    assertDeepEquals(
        {height: 5, width: 5}, size, 'Half mock size used for video');
    assertEquals(10, interval, 'Same mock interval value used for video');
    assertEquals(2, numFrames, '2 frames requested for video');
  });

  test('displays a loading spinner button while capturing frames', async () => {
    avatarCameraElement =
        initElement(AvatarCameraElement, {mode: AvatarCameraMode.VIDEO});
    await waitAfterNextRender(avatarCameraElement);

    assertEquals(
        null, avatarCameraElement.shadowRoot?.getElementById('loadingButton'),
        'no loading button shown yet');

    avatarCameraElement?.shadowRoot?.getElementById('takePhoto')?.click();
    await mockWebcamUtils.whenCalled('captureFrames');

    const loadingButton = avatarCameraElement.shadowRoot?.getElementById(
                              'loadingButton') as HTMLButtonElement;
    assertTrue(!!loadingButton, 'loading button is shown');
    assertTrue(loadingButton.disabled, 'loading button is disabled');

    await waitAfterNextRender(avatarCameraElement);

    assertEquals(
        'none', loadingButton.style.display, 'loading button hidden again');
  });

  test('calls saveCameraImage with data on confirmPhoto click', async () => {
    avatarCameraElement =
        initElement(AvatarCameraElement, {mode: AvatarCameraMode.CAMERA});
    await waitAfterNextRender(avatarCameraElement);

    avatarCameraElement.shadowRoot?.getElementById('takePhoto')?.click();
    await mockWebcamUtils.whenCalled('captureFrames');
    await waitAfterNextRender(avatarCameraElement);

    avatarCameraElement.shadowRoot?.getElementById('confirmPhoto')?.click();

    const bigBuffer = await userProvider.whenCalled('selectCameraImage');
    assertEquals(
        10, bigBuffer.sharedMemory.size,
        'camera data should be the right size for the mock data');

    const {buffer, result: mapBufferResult} =
        bigBuffer.sharedMemory.bufferHandle.mapBuffer(0, 10);
    assertEquals(
        Mojo.RESULT_OK, mapBufferResult,
        'Map buffer to read the image data back should succeed');

    const uint8View = new Uint8Array(buffer);
    assertTrue(uint8View.every(val => val === 17), 'mock data should be set');
  });
});
