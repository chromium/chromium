// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {PairingAuthType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {KeyEnteredHandlerRemote, PairingResult} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import type {BluetoothDeviceProperties, DevicePairingDelegateInterface, DevicePairingHandlerInterface, KeyEnteredHandlerPendingReceiver} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

/**
 * @fileoverview Fake implementation of DevicePairingHandler for testing.
 */
export class FakeDevicePairingHandler implements DevicePairingHandlerInterface {
  devicePairingDelegate: DevicePairingDelegateInterface|null = null;
  pairDeviceCallback: Function|null = null;
  pairDeviceRejectCallback: Function|null = null;
  fetchDeviceCallback: Function|null = null;
  pairDeviceCalledCount: number = 0;
  pinOrPasskey: string = '';
  confirmPasskeyResult: boolean = false;
  lastKeyEnteredHandlerRemote: KeyEnteredHandlerRemote|null = null;
  waitForPairDeviceCallback: Function|null = null;
  waitForFetchDeviceCallback: Function|null = null;
  finishRequestConfirmPasskeyCallback: Function|null = null;

  pairDevice(deviceId: string, delegate: DevicePairingDelegateInterface):
      Promise<any> {
    assert(deviceId);
    this.pairDeviceCalledCount++;
    this.devicePairingDelegate = delegate;
    const promise = new Promise((resolve, reject) => {
      this.pairDeviceCallback = resolve;
      this.pairDeviceRejectCallback = reject;
    });

    if (this.waitForPairDeviceCallback) {
      this.waitForPairDeviceCallback();
    }

    return promise;
  }

  fetchDevice(deviceAddress: string): Promise<any> {
    assert(deviceAddress);
    if (this.waitForFetchDeviceCallback) {
      this.waitForFetchDeviceCallback();
    }

    return new Promise((resolve) => {
      this.fetchDeviceCallback = resolve;
    });
  }

  /**
   * Returns a promise that will be resolved the next time
   * pairDevice() is called.
   */
  waitForPairDevice(): Promise<any> {
    return new Promise((resolve) => {
      this.waitForPairDeviceCallback = resolve;
    });
  }

  /**
   * Second step in pair device operation. This method should be called
   * after pairDevice(). Pass in a |PairingAuthType| to simulate each
   * pairing request made to |DevicePairingDelegate|.
   */
  requireAuthentication(authType: PairingAuthType, pairingCode?: string): void {
    assert(this.devicePairingDelegate, 'devicePairingDelegate was not set.');
    switch (authType) {
      case PairingAuthType.REQUEST_PIN_CODE:
        this.devicePairingDelegate!.requestPinCode()
            .then(
                (response) => this.finishRequestPinOrPasskey(response.pinCode))
            .catch(() => {});
        break;
      case PairingAuthType.REQUEST_PASSKEY:
        this.devicePairingDelegate!.requestPasskey()
            .then(
                (response) => this.finishRequestPinOrPasskey(response.passkey))
            .catch(() => {});
        break;
      case PairingAuthType.DISPLAY_PIN_CODE:
        assert(pairingCode);
        this.devicePairingDelegate!.displayPinCode(
            pairingCode!, this.getKeyEnteredHandlerPendingReceiver());
        break;
      case PairingAuthType.DISPLAY_PASSKEY:
        assert(pairingCode);
        this.devicePairingDelegate!.displayPasskey(
            pairingCode!, this.getKeyEnteredHandlerPendingReceiver());
        break;
      case PairingAuthType.CONFIRM_PASSKEY:
        assert(pairingCode);
        this.devicePairingDelegate!.confirmPasskey(pairingCode!)
            .then(
                (response) =>
                    this.finishRequestConfirmPasskey(response.confirmed))
            .catch(() => {});
        break;
      case PairingAuthType.AUTHORIZE_PAIRING:
        // TODO(crbug.com/1010321): Implement this.
        break;
    }
  }

  /**
   * Returns a promise that will be resolved the next time
   * finishRequestConfirmPasskey() is called.
   */
  waitForFinishRequestConfirmPasskey(): Promise<any> {
    return new Promise((resolve) => {
      this.finishRequestConfirmPasskeyCallback = resolve;
    });
  }

  private getKeyEnteredHandlerPendingReceiver():
      KeyEnteredHandlerPendingReceiver {
    this.lastKeyEnteredHandlerRemote = new KeyEnteredHandlerRemote();
    return this.lastKeyEnteredHandlerRemote!.$.bindNewPipeAndPassReceiver();
  }

  getLastKeyEnteredHandlerRemote(): KeyEnteredHandlerRemote {
    return this.lastKeyEnteredHandlerRemote!;
  }

  private finishRequestPinOrPasskey(code: string): void {
    this.pinOrPasskey = code;
  }

  private finishRequestConfirmPasskey(confirmed: boolean): void {
    this.confirmPasskeyResult = confirmed;

    if (this.finishRequestConfirmPasskeyCallback) {
      this.finishRequestConfirmPasskeyCallback();
    }
  }

  /**
   * Ends the operation to pair a bluetooth device. This method should be
   * called after pairDevice() has been called. It can be called without
   * calling pairingRequest() method to simulate devices being paired with
   * no pin or passkey required.
   */
  completePairDevice(success: boolean): void {
    assert(this.pairDeviceCallback, 'pairDevice() was never called.');
    this.pairDeviceCallback!(
        {result: success ? PairingResult.kSuccess : PairingResult.kAuthFailed});
  }

  /**
   * Simulates pairing failing due to an exception, such as the Mojo pipe
   * disconnecting.
   */
  rejectPairDevice(): void {
    if (this.pairDeviceRejectCallback) {
      this.pairDeviceRejectCallback();
    }
  }

  getPairDeviceCalledCount(): number {
    return this.pairDeviceCalledCount;
  }

  getPinOrPasskey(): string {
    return this.pinOrPasskey;
  }

  getConfirmPasskeyResult(): boolean {
    return this.confirmPasskeyResult;
  }

  getLastPairingDelegate(): DevicePairingDelegateInterface {
    return this.devicePairingDelegate!;
  }

  /**
   * Returns a promise that will be resolved the next time
   * fetchDevice() is called.
   */
  waitForFetchDevice(): Promise<any> {
    return new Promise((resolve) => {
      this.waitForFetchDeviceCallback = resolve;
    });
  }

  async completeFetchDevice(device: BluetoothDeviceProperties|
                            null): Promise<void> {
    if (!this.fetchDeviceCallback) {
      await this.waitForFetchDevice();
    }
    this.fetchDeviceCallback!({device: device});
  }
}
