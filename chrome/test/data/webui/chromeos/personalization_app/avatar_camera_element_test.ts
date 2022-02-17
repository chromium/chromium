// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AvatarCamera} from 'chrome://personalization/trusted/user/avatar_camera_element.js';
import * as webcamUtils from 'chrome://resources/cr_elements/chromeos/cr_picture/webcam_utils.js';
import {assertDeepEquals, assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestUserProvider} from './test_user_interface_provider';

type WebcamUtilsInterface = typeof webcamUtils;

class MockWebcamUtils extends TestBrowserProxy implements WebcamUtilsInterface {
  public captureFramesResponse = [];
  public pngUint8Array = new Uint8Array(10);

  CAPTURE_SIZE = {height: 10, width: 10};
  CAPTURE_INTERVAL_MS = 10;
  CAPTURE_DURATION_MS = 20;
  kDefaultVideoConstraints = webcamUtils.kDefaultVideoConstraints;

  constructor() {
    super(['captureFrames', 'stopMediaTracks', 'convertFramesToPngBinary']);
    this.pngUint8Array.fill(17);
  }

  convertFramesToPngBinary(frames: Array<HTMLCanvasElement>): Uint8Array {
    this.methodCalled('convertFramesToPngBinary', frames);
    return this.pngUint8Array;
  }

  convertFramesToPng(_: Array<HTMLCanvasElement>): string {
    assertNotReached('This function should never be called');
    return '';
  }

  async captureFrames(
      video: HTMLVideoElement, captureSize: typeof webcamUtils.CAPTURE_SIZE,
      intervalMs: number,
      numFrames: number): Promise<Array<HTMLCanvasElement>> {
    this.methodCalled(
        'captureFrames', video, captureSize, intervalMs, numFrames);
    return Promise.resolve(this.captureFramesResponse);
  }

  stopMediaTracks(stream: MediaStream|null): void {
    this.methodCalled('stopMediaTracks', stream);
  }
}

export function AvatarCameraTest() {
  let avatarCameraElement: AvatarCamera|null = null;
  let getUserMediaPromise: Promise<void>;
  let userProvider: TestUserProvider;
  const mockMediaStream = new MediaStream();


  setup(function() {
    AvatarCamera.webcamUtils = new MockWebcamUtils();
    getUserMediaPromise = new Promise((resolve) => {
      AvatarCamera.getUserMedia = async () => {
        resolve();
        return mockMediaStream;
      };
    });
    const mocks = baseSetup();
    userProvider = mocks.userProvider;
  });

  teardown(async () => {
    await teardownElement(avatarCameraElement);
    avatarCameraElement = null;
  });

  test('requests webcam media when open and attaches to video', async () => {
    avatarCameraElement = initElement(AvatarCamera, {open: false});
    await getUserMediaPromise;
    const video = avatarCameraElement.shadowRoot!.getElementById(
                      'webcamVideo') as HTMLVideoElement;
    assertEquals(
        mockMediaStream, video.srcObject,
        'video.srcObject should equal media stream object');
  });

  test('calls captureFrames and sends to mojom on click', async () => {
    avatarCameraElement = initElement(AvatarCamera, {open: false});
    await waitAfterNextRender(avatarCameraElement);

    avatarCameraElement.shadowRoot?.getElementById('takePhoto')?.click();

    const [video, size, interval, numFrames] =
        await (AvatarCamera.webcamUtils as MockWebcamUtils)
            .whenCalled('captureFrames');

    assertEquals(
        avatarCameraElement.shadowRoot?.getElementById('webcamVideo'), video,
        'Video element sent to captureFrames');

    assertDeepEquals({height: 10, width: 10}, size, 'Mock size used');
    assertEquals(10, interval, 'Mock interval value used');
    assertEquals(1, numFrames, 'Single frame requested for photo');

    const bigBuffer = await userProvider.whenCalled('selectCameraImage');
    assertEquals(
        10, bigBuffer.sharedMemory.size,
        'camera data should be the right size');

    const {buffer, result: mapBufferResult} =
        bigBuffer.sharedMemory.bufferHandle.mapBuffer(0, 10);
    assertEquals(
        Mojo.RESULT_OK, mapBufferResult,
        'Map buffer to read the image data back');

    const uint8View = new Uint8Array(buffer);
    assertTrue(uint8View.every(val => val === 17), 'mock data should be set');
  });
}
