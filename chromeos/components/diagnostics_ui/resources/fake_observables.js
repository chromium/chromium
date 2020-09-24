// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * @fileoverview
 * Implements a helper class for faking asynchronous observables.
 */

/**
 * Maintains state about an observable and the data it will produce.
 * @template T
 **/
class FakeObservableState {
  constructor() {
    /**
     * The list of functions that will be notified when the observable
     * is triggered.
     * @private {!Array<!function(!T)>}
     **/
    this.observers_ = [];

    /**
     * Array of observations to be supplied by the observer.
     * @private {!Array<T>}
     **/
    this.observations_ = [];

    /**
     * The index of the next observation.
     * @private {number}
     **/
    this.index_ = -1;

    /**
     * Id of the timer if enabled.
     * @private {number}
     */
    this.timerId_ = -1;
  }

  /** @param {!Array<!T>} observations */
  setObservableData(observations) {
    this.observations_ = observations;
    this.index_ = 0;
  }

  /** @param {!function(!T)} callback */
  addObserver(callback) {
    this.observers_.push(callback);
  }

  /**
   * Start firing the observers on a fixed interval. setObservableData() must
   * already have been called.
   * @param {number} intervalMs
   */
  startTriggerOnInterval(intervalMs) {
    assert(this.index_ >= 0);
    if (this.timerId_ != -1) {
      this.stopTriggerOnInterval();
    }

    assert(this.timerId_ == -1);
    this.timerId_ = setInterval(this.trigger.bind(this), intervalMs);
  }

  /**
   * Disables the observer firing automatically on an interval.
   */
  stopTriggerOnInterval() {
    if (this.timerId_ != -1) {
      clearInterval(this.timerId_);
      this.timerId_ = -1;
    }
  }

  /**
   * Causes the observable to trigger and notify all observers of the next
   * observation value.
   */
  trigger() {
    assert(this.observations_.length > 0);
    assert(this.index_ >= 0);
    assert(this.index_ < this.observations_.length);

    // Get the value of this observation and update the index to point to the
    // next one.
    const value = this.observations_[this.index_];
    this.index_ = (this.index_ + 1) % this.observations_.length;

    // Fire all the callbacks that are observing this observable.
    for (const fn of this.observers_) {
      fn(value);
    }
  }
}

/**
 * Manages a map of fake observables and the fake data they will produce
 * when triggered.
 * @template T
 */
export class FakeObservables {
  constructor() {
    /** @private {!Map<string, !FakeObservableState>} */
    this.observables_ = new Map();
  }

  /**
   * Register an observable. Other calls to this class will assert if the
   * observable has not been registered.
   * @param {string} methodName
   */
  register(methodName) {
    this.observables_.set(methodName, new FakeObservableState());
  }

  /**
   * Supply the callback for observing methodName.
   * @param {string} methodName
   * @param {!function(!T)} callback
   */
  observe(methodName, callback) {
    this.getObservable_(methodName).addObserver(callback);
  }

  /**
   * Sets the data that will be produced when the observable is triggered.
   * Each observation produces the next value in the array and wraps around
   * when all observations have been produced.
   * @param {string} methodName
   * @param {!Array<!T>} observations
   */
  setObservableData(methodName, observations) {
    this.getObservable_(methodName).setObservableData(observations);
  }
  /**
   * Start firing the observer on a fixed interval. setObservableData() must
   * already have been called.
   * @param {string} methodName
   * @param {number} intervalMs
   */
  startTriggerOnInterval(methodName, intervalMs) {
    this.getObservable_(methodName).startTriggerOnInterval(intervalMs);
  }

  /**
   * Disables the observer firing automatically on an interval.
   * @param {string} methodName
   */
  stopTriggerOnInterval(methodName) {
    this.getObservable_(methodName).stopTriggerOnInterval();
  }

  /**
   * Disables all observers firing automatically on an interval.
   */
  stopAllTriggerIntervals() {
    for (let obs of this.observables_.values()) {
      obs.stopTriggerOnInterval();
    }
  }

  /**
   * Causes the observable to trigger and notify all observers of the next
   * observation value.
   * @param {string} methodName
   */
  trigger(methodName) {
    this.getObservable_(methodName).trigger();
  }

  /**
   * Return the Observable for methodName.
   * @param {string} methodName
   * @return {!FakeObservableState}
   * @private
   */
  getObservable_(methodName) {
    let observable = this.observables_.get(methodName);
    assert(!!observable, `Observable '${methodName}' not found.`);
    return observable;
  }
}
