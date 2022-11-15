// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Contains fake implementations of mojo interfaces. */

import {TestBrowserProxy} from '../chromeos/test_browser_proxy.js';

/**
 * @implements {nearbyShare.mojom.ConfirmationManagerInterface}
 * @extends {TestBrowserProxy}
 */
export class FakeConfirmationManagerRemote extends TestBrowserProxy {
  constructor() {
    super([
      'accept',
      'reject',
      'cancel',
    ]);
  }

  async accept() {
    this.methodCalled('accept');
    return {success: true};
  }

  async reject() {
    this.methodCalled('reject');
    return {success: true};
  }

  async cancel() {
    this.methodCalled('cancel');
    return {success: true};
  }
}

/**
 * @implements {nearbyShare.mojom.DiscoveryManagerInterface}
 * @extends {TestBrowserProxy}
 */
export class FakeDiscoveryManagerRemote extends TestBrowserProxy {
  constructor() {
    super([
      'getPayloadPreview',
      'selectShareTarget',
      'startDiscovery',
      'stopDiscovery',
      'addDiscoveryObserver',
    ]);

    this.selectShareTargetResult = {
      result: nearbyShare.mojom.SelectShareTargetResult.kOk,
      transferUpdateListener: null,
      confirmationManager: null,
    };
    this.shareDescription = 'Test is a test share';
    this.startDiscoveryResult = nearbyShare.mojom.StartDiscoveryResult.kSuccess;
    /** @private {!nearbyShare.mojom.DiscoveryObserverInterface} */
    this.observer_;
  }

  /**
   * @return {!Promise<{payloadPreview: !nearbyShare.mojom.PayloadPreview}>}
   */
  async getPayloadPreview() {
    this.methodCalled('getPayloadPreview');
    return {
      payloadPreview: /** @type {!nearbyShare.mojom.PayloadPreview} */ ({
        description: this.shareDescription,
        fileCount: 0,
        shareType: 0,
      }),
    };
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} shareTargetId
   * @suppress {checkTypes} FakeConfirmationManagerRemote does not extend
   * ConfirmationManagerRemote but implements ConfirmationManagerInterface.
   */
  async selectShareTarget(shareTargetId) {
    this.methodCalled('selectShareTarget', shareTargetId);
    return this.selectShareTargetResult;
  }

  /**
   * @param {nearbyShare.mojom.ShareTargetListenerRemote} listener
   */
  async startDiscovery(listener) {
    this.methodCalled('startDiscovery', listener);
    return {result: this.startDiscoveryResult};
  }

  async stopDiscovery() {
    this.methodCalled('stopDiscovery');
  }

  /**
   * @param {!nearbyShare.mojom.DiscoveryObserverRemote} observer
   */
  addDiscoveryObserver(observer) {
    this.methodCalled('addDiscoveryObserver');
    this.observer_ = observer;
  }
}

/**
 * @extends {nearbyShare.mojom.TransferUpdateListenerPendingReceiver}
 */
export class FakeTransferUpdateListenerPendingReceiver extends
    nearbyShare.mojom.TransferUpdateListenerPendingReceiver {
  constructor() {
    const {handle0, handle1} = Mojo.createMessagePipe();
    super(handle0);
    this.remote_ = new nearbyShare.mojom.TransferUpdateListenerRemote(handle1);
  }
}
