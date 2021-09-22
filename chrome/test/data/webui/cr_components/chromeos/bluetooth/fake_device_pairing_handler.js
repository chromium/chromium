// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';

/**
 * @fileoverview Fake implementation of DevicePairingHandler for testing.
 */

/**
 * Device pairing request type. During device pairing,
 * this is passed to simulate a pairing request sent to
 * |DevicePairingDelegate|.
 * @enum {number}
 */
export const PairingRequestType = {
  REQUEST_PIN_CODE: 1,
  REQUEST_PASSKEY: 2,
  DISPLAY_PIN_CODE: 3,
  DISPLAY_PASS_KEY: 4,
  CONFIRM_PASSKEY: 5,
  AUTHORIZE_PAIRING: 6,
};

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
   * after pairDevice(). Pass in a |PairingRequestType| to simulate each
   * pairing request made to |DevicePairingDelegate|.
   * @param {!PairingRequestType} requestType
   */
  requireAuthorization(requestType) {
    switch (requestType) {
      case PairingRequestType.REQUEST_PIN_CODE:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingRequestType.REQUEST_PASSKEY:
        // TODO(crbug.com/1010321): Implement this
        break;
      case PairingRequestType.DISPLAY_PIN_CODE:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingRequestType.DISPLAY_PASS_KEY:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingRequestType.CONFIRM_PASSKEY:
        // TODO(crbug.com/1010321): Implement this.
        break;
      case PairingRequestType.AUTHORIZE_PAIRING:
        // TODO(crbug.com/1010321): Implement this.
        break;
    }
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
}
