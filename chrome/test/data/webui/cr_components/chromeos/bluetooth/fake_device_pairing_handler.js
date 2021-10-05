// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';
import {PairingAuthType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';

/**
 * @fileoverview Fake implementation of DevicePairingHandler for testing.
 */

/**
 * @implements {chromeos.bluetoothConfig.mojom.DevicePairingHandlerInterface}
 */
export class FakeDevicePairingHandler {
  constructor() {
    /**
     * @private {?chromeos.bluetoothConfig.mojom.DevicePairingDelegateInterface}
     */
    this.devicePairingDelegate_ = null;

    /**
     * @private {?function({result:
     *     chromeos.bluetoothConfig.mojom.PairingResult})}
     */
    this.pairDeviceCallback_ = null;

    /** @private {number} */
    this.pairDeviceCalledCount_ = 0;

    /** @private {string} */
    this.pinOrPasskey_ = '';
  }

  /** @override */
  pairDevice(deviceId, delegate) {
    this.pairDeviceCalledCount_++;
    this.devicePairingDelegate_ = delegate;
    let promise = new Promise((resolve, reject) => {
      this.pairDeviceCallback_ = resolve;
    });
    return promise;
  }

  /**
   * Second step in pair device operation. This method should be called
   * after pairDevice(). Pass in a |PairingAuthType| to simulate each
   * pairing request made to |DevicePairingDelegate|.
   * @param {!PairingAuthType} authType
   */
  requireAuthentication(authType) {
    switch (authType) {
      case PairingAuthType.REQUEST_PIN_CODE:
        this.devicePairingDelegate_.requestPinCode()
            .then(
                (response) => this.finishRequestPinOrPasskey_(response.pinCode))
            .catch(e => {});
        break;
      case PairingAuthType.REQUEST_PASSKEY:
        this.devicePairingDelegate_.requestPasskey()
            .then(
                (response) => this.finishRequestPinOrPasskey_(response.passkey))
            .catch(e => {});
        break;
      case PairingAuthType.DISPLAY_PIN_CODE:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingAuthType.DISPLAY_PASSKEY:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingAuthType.CONFIRM_PASSKEY:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingAuthType.AUTHORIZE_PAIRING:
        // TODO(crbug.com/1010321): Implement this.
        break;
    }
  }

  /**
   * @param {string} code
   * @private
   */
  finishRequestPinOrPasskey_(code) {
    this.pinOrPasskey_ = code;
  }

  /**
   * Ends the operation to pair a bluetooth device. This method should be called
   * after pairDevice() has been called. It can be called without calling
   * pairingRequest() method to simulate devices being paired with no pin or
   * passkey required.
   * @param {boolean} success
   */
  completePairDevice(success) {
    this.pairDeviceCallback_({
      result: success ? chromeos.bluetoothConfig.mojom.PairingResult.kSuccess :
                        chromeos.bluetoothConfig.mojom.PairingResult.kAuthFailed
    });
  }

  /**
   * @return {number}
   */
  getPairDeviceCalledCount() {
    return this.pairDeviceCalledCount_;
  }

  /** @return {string} */
  getPinOrPasskey() {
    return this.pinOrPasskey_;
  }
}
