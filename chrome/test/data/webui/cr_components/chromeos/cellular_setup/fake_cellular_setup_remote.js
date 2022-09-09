// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {ash.cellularSetup.mojom.CarrierPortalHandlerInterface}
 */
export class FakeCarrierPortalHandlerRemote {
  constructor() {}

  /** @override */
  onCarrierPortalStatusChange(status) {
    this.status_ = status;
  }
}

/** @implements {ash.cellularSetup.mojom.CellularSetupInterface} */
export class FakeCellularSetupRemote {
  /**
   * @param {!FakeCarrierPortalHandlerRemote} handler
   */
  constructor(handler) {
    this.carrierHandler_ = handler;
  }

  /** @override */
  startActivation(delegate) {
    this.delegate_ = delegate;
    return new Promise((resolve, reject) => {
      setTimeout(function() {
        resolve({observer: this.carrierHandler_});
      });
    });
  }

  /**
   * @returns {!ash.cellularSetup.mojom.ActivationDelegateRemote}
   */
  getLastActivationDelegate() {
    return this.delegate_;
  }
}
