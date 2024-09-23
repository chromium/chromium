// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of MediaDevices for testing.
 */

export class FakeMediaDevices implements MediaDevices {
  private devices_: MediaDeviceInfo[] = [];
  private deviceChangeListeners_: EventListener[] = [];

  addEventListener(_type: string, listener: EventListener): void {
    this.deviceChangeListeners_.push(listener);
  }

  enumerateDevices(): Promise<MediaDeviceInfo[]> {
    return Promise.resolve(this.devices_);
  }

  /**
   * Adds a media device to the list of media devices.
   */
  addDevice(kind: string, label: string): void {
    const device = {
      deviceId: '',
      kind: kind,
      label: label,
      groupId: '',
    } as MediaDeviceInfo;
    // https://w3c.github.io/mediacapture-main/#dom-mediadeviceinfo
    (device as any).__proto__ = MediaDeviceInfo.prototype;
    this.devices_.push(device);
    for (const deviceChangeListener of this.deviceChangeListeners_) {
      if (deviceChangeListener) {
        deviceChangeListener(new Event('addDevice'));
      }
    }
  }

  /**
   * Removes the most recently added media device from the list of media
   * devices.
   */
  popDevice(): void {
    this.devices_.pop();
    for (const deviceChangeListener of this.deviceChangeListeners_) {
      if (deviceChangeListener) {
        deviceChangeListener(new Event('popDevice'));
      }
    }
  }

  getDisplayMedia(): Promise<MediaStream> {
    return Promise.resolve(new MediaStream());
  }

  getUserMedia(): Promise<MediaStream> {
    return Promise.resolve(new MediaStream());
  }

  dispatchEvent(_event: Event): boolean {
    return false;
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

  ondevicechange(): void {}

  removeEventListener(): void {}
}
