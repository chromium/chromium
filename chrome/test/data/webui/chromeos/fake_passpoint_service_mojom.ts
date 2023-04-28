// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of PasspointService for testing.
 */
import {assert} from 'chrome://resources/ash/common/assert.js';
import {PasspointEventsListenerRemote, PasspointServiceInterface, PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';

export class FakePasspointService implements PasspointServiceInterface {
  private subs_: Map<string, PasspointSubscription>;

  constructor() {
    this.subs_ = new Map();
  }

  addSubscription(sub: PasspointSubscription): void {
    assert(sub !== undefined);
    this.subs_.set(sub.id, sub);
  }

  resetForTest(): void {
    this.subs_ = new Map();
  }

  getPasspointSubscription(id: string):
      Promise<{result: PasspointSubscription | null}> {
    return new Promise(resolve => {
      const sub = this.subs_.get(id);
      resolve({result: sub ? sub : null});
    });
  }

  listPasspointSubscriptions(): Promise<{result: PasspointSubscription[]}> {
    return Promise.resolve(
        {result: Array.from(this.subs_, ([_, value]) => (value))});
  }

  registerPasspointListener(_: PasspointEventsListenerRemote) {
    // Listener is ignored for now.
  }

  deletePasspointSubscription(id: string): Promise<{success: boolean}> {
    return new Promise(resolve => {
      resolve({success: this.subs_.delete(id)});
    });
  }
}
