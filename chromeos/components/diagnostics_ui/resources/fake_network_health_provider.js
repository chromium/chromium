// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkGuidInfo, NetworkHealthProviderInterface} from './diagnostics_types.js';
import {FakeObservables} from './fake_observables.js';

// Method names.
const ON_NETWORK_LIST_CHANGED_METHOD_NAME =
    'NetworkListObserver_onNetworkListChanged';

/**
 * @fileoverview
 * Implements a fake version of the NetworkHealthProvider mojo interface.
 */

/** @implements {NetworkHealthProviderInterface} */
export class FakeNetworkHealthProvider {
  constructor() {
    /** @private {!FakeObservables} */
    this.observables_ = new FakeObservables();

    this.registerObservables();
  }

  /*
   * Implements NetworkHealthProviderInterface.ObserveNetworkList.
   * @param {!NetworkListObserver} remote
   * @return {!Promise}
   */
  observeNetworkList(remote) {
    return this.observe_(ON_NETWORK_LIST_CHANGED_METHOD_NAME, (networkGuid) => {
      remote.onNetworkListChanged(
          /** @type {!NetworkGuidInfo} */ (networkGuid));
    });
  }

  /**
   * Sets the values that will be observed from observeNetworkList.
   * @param {!Array<!NetworkGuidInfo>} networkGuidInfoList
   */
  setFakeNetworkGuidInfo(networkGuidInfoList) {
    this.observables_.setObservableData(
        ON_NETWORK_LIST_CHANGED_METHOD_NAME, networkGuidInfoList);
  }

  /**
   * Causes the network list observer to fire.
   */
  triggerNetworkListObserver() {
    this.observables_.trigger(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
  }

  /**
   * Make the observable fire automatically on provided interval.
   * @param {string} methodName
   * @param {number} intervalMs
   */
  startTriggerInterval(methodName, intervalMs) {
    this.observables_.startTriggerOnInterval(methodName, intervalMs);
  }

  /**
   * Stop automatically triggering observables.
   */
  stopTriggerIntervals() {
    this.observables_.stopAllTriggerIntervals();
  }

  registerObservables() {
    this.observables_.register(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
    this.observables_.stopAllTriggerIntervals();
    this.observables_ = new FakeObservables();
    this.registerObservables();
  }

  /*
   * Sets up an observer for methodName.
   * @template T
   * @param {string} methodName
   * @param {!function(!T)} callback
   * @return {!Promise}
   * @private
   */
  observe_(methodName, callback) {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }
}
