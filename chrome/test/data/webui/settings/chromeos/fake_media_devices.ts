// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of MediaDevices for testing.
 */

export class FakeMediaDevices implements MediaDevices {
  private devices_: MediaDeviceInfo[] = [];
  private deviceChangeListener_: EventListener|null = null;

  addEventListener(_type: string, listener: EventListener): void {
    this.deviceChangeListener_ = listener;
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
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_(new Event('addDevice'));
    }
  }

  /**
   * Removes the most recently added media device from the list of media
   * devices.
   */
  popDevice(): void {
    this.devices_.pop();
    if (this.deviceChangeListener_) {
      this.deviceChangeListener_(new Event('popDevice'));
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

  getSupportedConstraints(): object {
    return {};
  }

  ondevicechange(): void {}

  removeEventListener(): void {}
}
