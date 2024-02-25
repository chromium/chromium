// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

export class FakeMediaDevices implements MediaDevices {
  isStreamingUserFacingCamera: boolean = true;
  private devices_: MediaDeviceInfo[] = [];
  private deviceChangeListener_: EventListener|null = null;
  private enumerateDevicesResolver_: Function|null = null;
  private getMediaDevicesResolver_: Function|null = null;

  addEventListener(_type: string, listener: EventListener): void {
    this.deviceChangeListener_ = listener;
  }

  enumerateDevices(): Promise<MediaDeviceInfo[]> {
    return new Promise((res, _rej) => {
      this.enumerateDevicesResolver_ = res;
    });
  }

  resolveEnumerateDevices(callback: Function): void {
    assertTrue(
        !!this.enumerateDevicesResolver_, 'enumerateDevices was not called');
    this.enumerateDevicesResolver_(this.devices_);
    callback();
  }

  getSupportedConstraints(): MediaTrackSupportedConstraints {
    return {
      whiteBalanceMode: false,
      exposureMode: false,
      focusMode: false,
      pointsOfInterest: false,

      exposureCompensation: false,
      colorTemperature: false,
      iso: false,

      brightness: false,
      contrast: false,
      saturation: false,
      sharpness: false,
      focusDistance: false,
      zoom: false,
      torch: false,
    };
  }

  getDisplayMedia(): Promise<MediaStream> {
    return Promise.resolve(new MediaStream());
  }

  getUserMedia(constraints: any): Promise<MediaStream> {
    this.isStreamingUserFacingCamera = constraints.video.facingMode === 'user';
    return new Promise((res, _rej) => {
      this.getMediaDevicesResolver_ = res;
    });
  }

  /**
   * Resolves promise returned from getUserMedia().
   */
  resolveGetUserMedia(): void {
    assertTrue(!!this.getMediaDevicesResolver_, 'getUserMedia was not called');
    this.getMediaDevicesResolver_(new MediaStream());
  }

  removeEventListener(): void {}

  /**
   * Adds a video input device to the list of media devices.
   */
  addDevice(): void {
    const device = {
      deviceId: '',
      kind: 'videoinput',
      label: '',
      groupId: '',
    } as MediaDeviceInfo;
    // https://w3c.github.io/mediacapture-main/#dom-mediadeviceinfo
    (device as any).__proto__ = MediaDeviceInfo.prototype;
    this.devices_.push(device);
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_(new Event('addDevice'));
    }
  }

  /**
   * Removes the most recently added media device from the list of media
   * devices.
   */
  removeDevice(): void {
    this.devices_.pop();
    if (this.devices_.length <= 1) {
      this.isStreamingUserFacingCamera = true;
    }
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_(new Event('removeDevice'));
    }
  }

  ondevicechange(): void {}

  dispatchEvent(_event: Event): boolean {
    return false;
  }
}
