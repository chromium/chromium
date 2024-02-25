// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeviceNameBrowserProxy, DeviceNameMetadata, DeviceNameState, SetDeviceNameResult} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDeviceNameBrowserProxy extends TestBrowserProxy implements
    DeviceNameBrowserProxy {
  private deviceName_ = '';
  private deviceNameResult_: SetDeviceNameResult =
      SetDeviceNameResult.UPDATE_SUCCESSFUL;

  constructor() {
    super([
      'notifyReadyForDeviceName',
      'attemptSetDeviceName',
    ]);
  }

  setDeviceNameResultForTesting(deviceNameResult: SetDeviceNameResult): void {
    this.deviceNameResult_ = deviceNameResult;
  }

  getDeviceName(): string {
    return this.deviceName_;
  }

  notifyReadyForDeviceName(): Promise<DeviceNameMetadata> {
    this.methodCalled('notifyReadyForDeviceName');
    return Promise.resolve({
      deviceName: this.deviceName_,
      deviceNameState: DeviceNameState.CAN_BE_MODIFIED,
    });
  }

  attemptSetDeviceName(name: string): Promise<SetDeviceNameResult> {
    if (this.deviceNameResult_ === SetDeviceNameResult.UPDATE_SUCCESSFUL) {
      this.deviceName_ = name;
    }

    this.methodCalled('attemptSetDeviceName');
    return Promise.resolve(this.deviceNameResult_);
  }
}
