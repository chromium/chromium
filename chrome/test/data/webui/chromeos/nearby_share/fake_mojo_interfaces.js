// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Contains fake implementations of mojo interfaces. */

import {DiscoveryObserverRemote, SelectShareTargetResult, ShareTargetListenerRemote, StartDiscoveryResult, TransferUpdateListenerPendingReceiver, TransferUpdateListenerRemote} from 'chrome://nearby/shared/nearby_share.mojom-webui.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {TestBrowserProxy} from '../test_browser_proxy.js';

/**
 * @implements {ConfirmationManagerInterface}
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
 * @implements {DiscoveryManagerInterface}
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
      result: SelectShareTargetResult.kOk,
      transferUpdateListener: null,
      confirmationManager: null,
    };
    this.shareDescription = 'Test is a test share';
    this.startDiscoveryResult = StartDiscoveryResult.kSuccess;
    /** @private {!DiscoveryObserverInterface} */
    this.observer_;
  }

  /**
   * @return {!Promise<{payloadPreview: !PayloadPreview}>}
   */
  async getPayloadPreview() {
    this.methodCalled('getPayloadPreview');
    return {
      payloadPreview: /** @type {!PayloadPreview} */ ({
        description: this.shareDescription,
        fileCount: 0,
        shareType: 0,
      }),
    };
  }

  /**
   * @param {!UnguessableToken} shareTargetId
   * @suppress {checkTypes} FakeConfirmationManagerRemote does not extend
   * ConfirmationManagerRemote but implements ConfirmationManagerInterface.
   */
  async selectShareTarget(shareTargetId) {
    this.methodCalled('selectShareTarget', shareTargetId);
    return this.selectShareTargetResult;
  }

  /**
   * @param {ShareTargetListenerRemote} listener
   */
  async startDiscovery(listener) {
    this.methodCalled('startDiscovery', listener);
    return {result: this.startDiscoveryResult};
  }

  async stopDiscovery() {
    this.methodCalled('stopDiscovery');
  }

  /**
   * @param {!DiscoveryObserverRemote} observer
   */
  addDiscoveryObserver(observer) {
    this.methodCalled('addDiscoveryObserver');
    this.observer_ = observer;
  }
}

/**
 * @extends {TransferUpdateListenerPendingReceiver}
 */
export class FakeTransferUpdateListenerPendingReceiver extends
    TransferUpdateListenerPendingReceiver {
  constructor() {
    const {handle0, handle1} = Mojo.createMessagePipe();
    super(handle0);
    this.remote_ = new TransferUpdateListenerRemote(handle1);
  }
}
