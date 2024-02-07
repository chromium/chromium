// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {Action} from 'chrome://resources/js/store.js';
import {Store} from 'chrome://resources/js/store.js';

import {assertTrue} from './chai_assert.js';

/**
 * This is a generic test store, designed to replace a real Store instance
 * during testing.
 */
export class TestStore<S, A extends Action = Action> extends Store<S, A> {
  private initPromise_: PromiseResolver<void>|null = null;
  private enableReducers_: boolean = false;
  private resolverMap_: Map<string, PromiseResolver<A>> = new Map();
  private lastAction_: A|null = null;

  constructor(
      initialData: Partial<S>, storeImplEmptyState: S,
      storeImplReducer: (state: S, action: A) => S) {
    super(storeImplEmptyState, storeImplReducer);

    this.data = Object.assign({}, this.data, initialData as S);
    this.initialized_ = true;
  }

  override init(state: S) {
    if (this.initPromise_) {
      super.init(state);
      this.initPromise_.resolve();
    }
  }

  get lastAction() {
    return this.lastAction_;
  }

  resetLastAction() {
    this.lastAction_ = null;
  }

  /**
   * Enable or disable calling the reducer for each action.
   * With reducers disabled (the default), TestStore is a stub which
   * requires state be managed manually (suitable for unit tests). With
   * reducers enabled, TestStore becomes a proxy for observing actions
   * (suitable for integration tests).
   */
  setReducersEnabled(enabled: boolean) {
    this.enableReducers_ = enabled;
  }

  override reduce(action: A) {
    this.lastAction_ = action;
    if (this.enableReducers_) {
      super.reduce(action);
    }
    if (this.resolverMap_.has(action.name)) {
      this.resolverMap_.get(action.name)!.resolve(action);
    }
  }

  /**
   * Notifies UI elements that the store data has changed. When reducers are
   * disabled, tests are responsible for manually changing the data to make
   * UI elements update correctly (eg, tests must replace the whole list
   * when changing a single element).
   */
  notifyObservers() {
    this.notifyObservers_(this.data);
  }

  /**
   * Call in order to accept data from an init call to the TestStore once.
   * @return Promise which resolves when the store is initialized.
   */
  acceptInitOnce(): Promise<void> {
    this.initPromise_ = new PromiseResolver<void>();
    this.initialized_ = false;
    return this.initPromise_.promise;
  }

  /**
   * Track actions called |name|, allowing that type of action to be waited
   * for with `waitForAction`.
   */
  expectAction(name: string) {
    this.resolverMap_.set(name, new PromiseResolver<A>());
  }

  /**
   * Returns a Promise that will resolve when an action called |name| is
   * dispatched. The promise must be prepared by calling
   * `expectAction(name)` before the action is dispatched.
   */
  async waitForAction(name: string): Promise<A> {
    assertTrue(
        this.resolverMap_.has(name),
        'Must call expectAction before each call to waitForAction');
    const action = await this.resolverMap_.get(name)!.promise;
    this.resolverMap_.delete(name);
    return action;
  }
}
