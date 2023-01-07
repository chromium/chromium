// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of MediaDevices for testing.
 */

/**
 * @implements {MediaDevices}
 */
export class FakeMediaDevices {
  constructor() {
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

  /**
   * Adds a media device to the list of media devices.
   * @param {!string} kind
   * @param {!string} label
   */
  addDevice(kind, label) {
    const device = {
      deviceId: '',
      kind: kind,
      label: label,
      groupId: '',
    };
    // https://w3c.github.io/mediacapture-main/#dom-mediadeviceinfo
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
  popDevice() {
    this.devices_.pop();
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_();
    }
  }
}
