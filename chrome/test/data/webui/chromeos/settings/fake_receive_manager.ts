// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of ReceiveManagerInterface for testing.
 */

import {nearbyShareMojom} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type ReceiveObserverInterface = nearbyShareMojom.ReceiveObserverInterface;
type ReceiveManagerInterface = nearbyShareMojom.ReceiveManagerInterface;
type ReceiveObserverRemote = nearbyShareMojom.ReceiveObserverRemote;
type RegisterReceiveSurfaceResult =
    nearbyShareMojom.RegisterReceiveSurfaceResult;
type ShareTarget = nearbyShareMojom.ShareTarget;
type TransferMetadata = nearbyShareMojom.TransferMetadata;

const {
  RegisterReceiveSurfaceResult,
  TransferStatus,
} = nearbyShareMojom;

/**
 * Fake implementation of ReceiveManagerInterface
 */
export class FakeReceiveManager extends TestBrowserProxy implements
    ReceiveManagerInterface {
  private inHighVisibility_ = false;
  private nextResult_ = true;
  private observer_: ReceiveObserverInterface|null = null;

  // Make this look like a closable mojo pipe
  $ = {
    close() {},
  };

  constructor() {
    super([
      'addReceiveObserver',
      'isInHighVisibility',
      'registerForegroundReceiveSurface',
      'unregisterForegroundReceiveSurface',
      'accept',
      'reject',
      'recordFastInitiationNotificationUsage',
    ]);
  }

  simulateShareTargetArrival(
      name: string, connectionToken: string, _payloadDescription = '',
      _payloadType = 0): ShareTarget {
    const target: ShareTarget = {
      id: (1n.toString(16).padStart(16, '0') +
           2n.toString(16).padStart(16, '0'))
              .toUpperCase(),
      name,
      type: 1,
      payloadPreview: {
        description: '',
        fileCount: 0,
        shareType: 0,
      },
      forSelfShare: false,
      imageUrl: null,
    };
    const metadata: TransferMetadata = {
      status: TransferStatus.kAwaitingLocalConfirmation,
      progress: 0.0,
      token: connectionToken,
      isOriginal: true,
      isFinalStatus: false,
    };
    this.observer_!.onTransferUpdate(target, metadata);
    return target;
  }

  addReceiveObserver(observer: ReceiveObserverRemote): void {
    this.methodCalled('addReceiveObserver');
    this.observer_ = observer;
  }

  isInHighVisibility(): Promise<{inHighVisibility: boolean}> {
    this.methodCalled('isInHighVisibility');
    return Promise.resolve({inHighVisibility: this.inHighVisibility_});
  }

  registerForegroundReceiveSurface():
      Promise<{result: RegisterReceiveSurfaceResult}> {
    this.inHighVisibility_ = true;
    if (this.observer_) {
      this.observer_.onHighVisibilityChanged(this.inHighVisibility_);
    }
    this.methodCalled('registerForegroundReceiveSurface');
    const result = this.nextResult_ ? RegisterReceiveSurfaceResult.kSuccess :
                                      RegisterReceiveSurfaceResult.kFailure;
    return Promise.resolve({result});
  }

  unregisterForegroundReceiveSurface(): Promise<{success: boolean}> {
    this.inHighVisibility_ = false;
    if (this.observer_) {
      this.observer_.onHighVisibilityChanged(this.inHighVisibility_);
    }
    this.methodCalled('unregisterForegroundReceiveSurface');
    return Promise.resolve({success: this.nextResult_});
  }

  accept(shareTargetId: string): Promise<{success: boolean}> {
    this.methodCalled('accept', shareTargetId);
    return Promise.resolve({success: this.nextResult_});
  }

  reject(shareTargetId: string): Promise<{success: boolean}> {
    this.methodCalled('reject', shareTargetId);
    return Promise.resolve({success: this.nextResult_});
  }

  recordFastInitiationNotificationUsage(success: boolean): void {
    this.methodCalled('recordFastInitiationNotificationUsage', success);
  }

  getInHighVisibilityForTest(): boolean {
    return this.inHighVisibility_;
  }

  setInHighVisibilityForTest(inHighVisibility: boolean): void {
    this.inHighVisibility_ = inHighVisibility;
    if (this.observer_) {
      this.observer_.onHighVisibilityChanged(inHighVisibility);
    }
  }

  setNextResultForTest(nextResult: boolean): void {
    this.nextResult_ = nextResult;
  }
}
