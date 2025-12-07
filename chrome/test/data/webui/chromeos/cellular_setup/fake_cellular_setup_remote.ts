// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {ActivationDelegateRemote, CarrierPortalHandlerInterface, CarrierPortalHandlerRemote, CarrierPortalStatus, CellularSetupInterface} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';

export class FakeCarrierPortalHandlerRemote implements
    CarrierPortalHandlerInterface {
  onCarrierPortalStatusChange(_status: CarrierPortalStatus) {}
}

export class FakeCellularSetupRemote implements CellularSetupInterface {
  private carrierHandler_: FakeCarrierPortalHandlerRemote;
  private delegate_: ActivationDelegateRemote|null = null;

  constructor(handler: FakeCarrierPortalHandlerRemote) {
    this.carrierHandler_ = handler;
  }

  startActivation(delegate: ActivationDelegateRemote):
      Promise<{observer: CarrierPortalHandlerRemote}> {
    this.delegate_ = delegate;
    return new Promise((resolve, _reject) => {
      setTimeout(() => {
        resolve({
          observer: this.carrierHandler_ as unknown as
              CarrierPortalHandlerRemote,
        });
      });
    });
  }

  getLastActivationDelegate(): ActivationDelegateRemote {
    assert(!!this.delegate_);
    return this.delegate_;
  }
}
