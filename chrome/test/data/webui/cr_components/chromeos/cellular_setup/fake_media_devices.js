// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /**
   * @implements {MediaDevices}
   */
  /* #export */ class FakeMediaDevices {
    constructor() {
      this.isStreamingUserFacingCamera = true;
      this.devices_ = [];
    }

    /** @override */
    addEventListener(type, listener) {
      this.deviceChangeListener_ = listener;
    }

    /** @override */
    enumerateDevices() {
      return new Promise((res, rej) => {
        res(this.devices_);
      });
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
      this.isStreamingUserFacingCamera =
          constraints.video.facingMode === 'user';
      return new Promise((res, rej) => {
        res(new MediaStream());
      });
    }

    /** @override */
    removeEventListener(event, fn) {}

    /**
     * Adds a video input device to the list of media devices.
     */
    addDevice() {
      let device = {
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

  // #cr_define_end
  return {
    FakeMediaDevices: FakeMediaDevices,
  };
});