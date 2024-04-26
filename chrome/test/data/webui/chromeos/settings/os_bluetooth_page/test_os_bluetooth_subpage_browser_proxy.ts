// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {FastPairSavedDevice, FastPairSavedDevicesOptInStatus, OsBluetoothDevicesSubpageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOsBluetoothDevicesSubpageBrowserProxy extends TestBrowserProxy
    implements OsBluetoothDevicesSubpageBrowserProxy {
  savedDevices: FastPairSavedDevice[] = [];
  optInStatus = FastPairSavedDevicesOptInStatus.STATUS_OPTED_IN;
  private isBatterySaverActive_: boolean = false;
  private isHardwareOffloadingSupported_: boolean = true;
  private showBluetoothRevampHatsSurveyCount_ = 0;
  constructor() {
    super([
      'deleteFastPairSavedDevice',
      'requestBatterySaverStatus',
      'requestFastPairSavedDevices',
      'requestFastPairDeviceSupport',
      'requestHardwareOffloadingSupportStatus',
    ]);
  }

  override reset() {
    super.reset();
    // reset instance variables
    this.savedDevices = [];
    this.optInStatus = FastPairSavedDevicesOptInStatus.STATUS_OPTED_IN;
  }

  deleteFastPairSavedDevice(accountKey: string): void {
    // Remove the device from the proxy's device list if it exists,
    this.savedDevices =
        this.savedDevices.filter(device => device.accountKey !== accountKey);
  }

  setBatterySaverStatus(isBatterySaverActive: boolean): void {
    this.isBatterySaverActive_ = isBatterySaverActive;
  }

  setHardwareOffloadingSupportStatus(isHardwareOffloadingSupported: boolean):
      void {
    this.isHardwareOffloadingSupported_ = isHardwareOffloadingSupported;
  }

  setSavedDevices(savedDevices: FastPairSavedDevice[]): void {
    this.savedDevices = savedDevices;
  }

  setOptInStatus(status: FastPairSavedDevicesOptInStatus): void {
    this.optInStatus = status;
  }

  requestBatterySaverStatus(): void {
    this.methodCalled('requestBatterySaverStatus');
    webUIListenerCallback(
        'fast-pair-software-scanning-battery-saver-status',
        this.isBatterySaverActive_);
  }

  requestFastPairDeviceSupport(): void {}

  requestFastPairSavedDevices(): void {
    this.methodCalled('requestFastPairSavedDevices');
    webUIListenerCallback('fast-pair-saved-devices-list', this.savedDevices);
    webUIListenerCallback(
        'fast-pair-saved-devices-opt-in-status', this.optInStatus);
  }

  requestHardwareOffloadingSupportStatus(): void {
    this.methodCalled('requestHardwareOffloadingSupportStatus');
    webUIListenerCallback(
        'fast-pair-software-scanning-hardware-offloading-status',
        this.isHardwareOffloadingSupported_);
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
