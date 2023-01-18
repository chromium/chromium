// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of ReceiveManagerInterface for testing.
 */

import {ReceiveManagerInterface, ReceiveObserverInterface, ReceiveObserverRemote, RegisterReceiveSurfaceResult, TransferStatus} from 'chrome://os-settings/mojo/nearby_share.mojom-webui.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Fake implementation of ReceiveManagerInterface
 *
 * @implements {ReceiveManagerInterface}
 */
export class FakeReceiveManager extends TestBrowserProxy {
  constructor() {
    super([
      'addReceiveObserver',
      'isInHighVisibility',
      'registerForegroundReceiveSurface',
      'unregisterForegroundReceiveSurface',
      'accept',
      'reject',
    ]);
    /** @private {!ReceiveObserverInterface} */
    this.observer_;
    /** @private {!boolean} */
    this.inHighVisibility_ = false;
    /** @private {?UnguessableToken} */
    this.lastToken_ = null;
    /** @private {!boolean} */
    this.nextResult_ = true;
    // Make this look like a closable mojo pipe
    /** @private {Object} */
    this.$ = {
      close() {},
    };
  }

  simulateShareTargetArrival(
      name, connectionToken, payloadDescription = '', payloadType = 0) {
    const target = {
      id: {low: 1, high: 2},
      name: name,
      type: 1,
      payloadPreview: {
        description: '',
        fileCount: 0,
        shareType: 0,
      },
    };
    const metadata = {
      'status': TransferStatus.kAwaitingLocalConfirmation,
      progress: 0.0,
      token: connectionToken,
      is_original: true,
      is_final_status: false,
    };
    this.observer_.onTransferUpdate(target, metadata);
    return target;
  }

  /**
   * @param {!ReceiveObserverRemote} observer
   */
  addReceiveObserver(observer) {
    this.methodCalled('addReceiveObserver');
    this.observer_ = observer;
  }

  /**
   * @return {!Promise<{inHighVisibility: !boolean}>}
   */
  async isInHighVisibility() {
    this.methodCalled('isInHighVisibility');
    return {inHighVisibility: this.inHighVisibility_};
  }

  /**
   * @return {!Promise<{result: !RegisterReceiveSurfaceResult}>}
   */
  async registerForegroundReceiveSurface() {
    this.inHighVisibility_ = true;
    if (this.observer_) {
      this.observer_.onHighVisibilityChanged(this.inHighVisibility_);
    }
    this.methodCalled('registerForegroundReceiveSurface');
    const result = this.nextResult_ ? RegisterReceiveSurfaceResult.kSuccess :
                                      RegisterReceiveSurfaceResult.kFailure;
    return {result: result};
  }

  /**
   * @return {!Promise<{success: !boolean}>}
   */
  async unregisterForegroundReceiveSurface() {
    this.inHighVisibility_ = false;
    if (this.observer_) {
      this.observer_.onHighVisibilityChanged(this.inHighVisibility_);
    }
    this.methodCalled('unregisterForegroundReceiveSurface');
    return {success: this.nextResult_};
  }

  /**
   * @param {!UnguessableToken} shareTargetId
   * @return {!Promise<{success: !boolean}>}
   */
  async accept(shareTargetId) {
    this.lastToken_ = shareTargetId;
    this.methodCalled('accept', shareTargetId);
    return {success: this.nextResult_};
  }

  /**
   * @param {!UnguessableToken} shareTargetId
   * @return {!Promise<{success: !boolean}>}
   */
  async reject(shareTargetId) {
    this.lastToken_ = shareTargetId;
    this.methodCalled('reject', shareTargetId);
    return {success: this.nextResult_};
  }

  /**
   * @return {boolean}
   */
  getInHighVisibilityForTest() {
    return this.inHighVisibility_;
  }

  /**
   * @param {boolean} inHighVisibility
   */
  setInHighVisibilityForTest(inHighVisibility) {
    this.inHighVisibility_ = inHighVisibility;
    if (this.observer_) {
      this.observer_.onHighVisibilityChanged(inHighVisibility);
    }
  }
}
