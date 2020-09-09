// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Contains fake implementations of mojo interfaces. */

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

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
      'getSendPreview',
      'selectShareTarget',
      'startDiscovery',
    ]);

    this.selectShareTargetResult = {
      result: nearbyShare.mojom.SelectShareTargetResult.kOk,
      transferUpdateListener: null,
      confirmationManager: null,
    };
    this.shareDescription = 'Test is a test share';
  }

  /**
   * @return {!Promise<{sendPreview: !nearbyShare.mojom.SendPreview}>}
   */
  async getSendPreview() {
    this.methodCalled('getSendPreview');
    return {
      sendPreview: /** @type {!nearbyShare.mojom.SendPreview} */ ({
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

  async startDiscovery(listener) {
    this.methodCalled('startDiscovery', listener);
    return {success: true};
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
