// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {FastPairSavedDevice, FastPairSavedDevicesOptInStatus, OsBluetoothDevicesSubpageBrowserProxy} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * @implements {OsBluetoothDevicesSubpageBrowserProxy}
 */
export class TestOsBluetoothDevicesSubpageBrowserProxy extends TestBrowserProxy
    implements OsBluetoothDevicesSubpageBrowserProxy {
  savedDevices: FastPairSavedDevice[] = [];
  optInStatus = FastPairSavedDevicesOptInStatus.STATUS_OPTED_IN;
  private showBluetoothRevampHatsSurveyCount_ = 0;
  constructor() {
    super([
      'requestFastPairSavedDevices',
      'deleteFastPairSavedDevice',
      'requestFastPairDeviceSupport',
    ]);
  }

  requestFastPairDeviceSupport(): void {}

  requestFastPairSavedDevices(): void {
    this.methodCalled('requestFastPairSavedDevices');
    webUIListenerCallback('fast-pair-saved-devices-list', this.savedDevices);
    webUIListenerCallback(
        'fast-pair-saved-devices-opt-in-status', this.optInStatus);
  }

  deleteFastPairSavedDevice(accountKey: string): void {
    // Remove the device from the proxy's device list if it exists,
    this.savedDevices =
        this.savedDevices.filter(device => device.accountKey !== accountKey);
  }

  showBluetoothRevampHatsSurvey(): void {
    this.showBluetoothRevampHatsSurveyCount_++;
  }

  /**
   * Returns the number of times showBluetoothRevampHatsSurvey()
   * was called.
   */
  getShowBluetoothRevampHatsSurveyCount(): number {
    return this.showBluetoothRevampHatsSurveyCount_;
  }
}
