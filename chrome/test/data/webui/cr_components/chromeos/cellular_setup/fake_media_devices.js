// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {MediaDevices}
 */
export class FakeMediaDevices {
  constructor() {
    this.isStreamingUserFacingCamera = true;
    this.devices_ = [];

    /** @private {?function()} */
    this.enumerateDevicesResolver_ = null;

    /** @private {?function()} */
    this.getMediaDevicesResolver_ = null;
  }

  /** @override */
  addEventListener(type, listener) {
    this.deviceChangeListener_ = listener;
  }

  /** @override */
  enumerateDevices() {
    return new Promise((res, rej) => {
      this.enumerateDevicesResolver_ = res;
    });
  }

  /**
   * Resolves promise returned from enumerateDevices().
   * @param {?function()} callback
   */
  resolveEnumerateDevices(callback) {
    assertTrue(
        !!this.enumerateDevicesResolver_, 'enumerateDevices was not called');
    this.enumerateDevicesResolver_(this.devices_);
    callback();
  }

  /** @override */
  getSupportedConstraints() {
    return null;
  }

  /** @override */
  getDisplayMedia() {
    return new Promise((res, rej) => {
      res(null);
    });
  }

  /** @override */
  getUserMedia(constraints) {
    this.isStreamingUserFacingCamera = constraints.video.facingMode === 'user';
    return new Promise((res, rej) => {
      this.getMediaDevicesResolver_ = res;
    });
  }

  /**
   * Resolves promise returned from getUserMedia().
   */
  resolveGetUserMedia() {
    assertTrue(!!this.getMediaDevicesResolver_, 'getUserMedia was not called');
    this.getMediaDevicesResolver_(new MediaStream());
  }

  /** @override */
  removeEventListener(event, fn) {}

  /**
   * Adds a video input device to the list of media devices.
   */
  addDevice() {
    const device = {
      deviceId: '',
      kind: 'videoinput',
      label: '',
      groupId: '',
    };
    device.__proto__ = MediaDeviceInfo.prototype;
    this.devices_.push(device);
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_();
    }
  }

  /**
   * Removes the most recently added media device from the list of media
   * devices.
   */
  removeDevice() {
    this.devices_.pop();
    if (this.devices_.length <= 1) {
      this.isStreamingUserFacingCamera = true;
    }
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_();
    }
  }
}
