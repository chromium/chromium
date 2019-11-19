// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
// #import {Store} from 'chrome://resources/js/cr/ui/store.m.js';

cr.define('cr.ui', function() {
  /**
   * This is a generic test store, designed to replace a real Store instance
   * during testing. It should be extended by providing the specific store
   * implementation class that the test store is replacing, as in
   * chrome/test/data/webui/bookmarks/test_store.js.
   */
  /* #export */ class TestStore extends cr.ui.Store {
    constructor(
        initialData, storeImplClass, storeImplEmptyState, storeImplReducer) {
      super(storeImplEmptyState, storeImplReducer);

      this.data = Object.assign(this.data, initialData);
      this.initialized_ = true;
      this.lastAction_ = null;
      /** @type {?PromiseResolver} */
      this.initPromise_ = null;
      this.enableReducers_ = false;
      /** @type {!Map<string, !PromiseResolver>} */
      this.resolverMap_ = new Map();
      /** @type {!cr.ui.Store} */
      this.storeImplClass = storeImplClass;
    }

    /** @override */
    init(state) {
      if (this.initPromise_) {
        this.storeImplClass.prototype.init.call(this, state);
        this.initPromise_.resolve();
      }
    }

    get lastAction() {
      return this.lastAction_;
    }

    resetLastAction() {
      this.lastAction_ = null;
    }

    /** Replace the global store instance with this TestStore. */
    replaceSingleton() {
      this.storeImplClass.instance_ = this;
    }

    /**
     * Enable or disable calling the reducer for each action.
     * With reducers disabled (the default), TestStore is a stub which
     * requires state be managed manually (suitable for unit tests). With
     * reducers enabled, TestStore becomes a proxy for observing actions
     * (suitable for integration tests).
     * @param {boolean} enabled
     */
    setReducersEnabled(enabled) {
      this.enableReducers_ = enabled;
    }

    /** @override */
    reduce_(action) {
      this.lastAction_ = action;
      if (this.enableReducers_) {
        this.storeImplClass.prototype.reduce_.call(this, action);
      }
      if (this.resolverMap_.has(action.name)) {
        this.resolverMap_.get(action.name).resolve(action);
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
     * @return {Promise} Promise which resolves when the store is initialized.
     */
    acceptInitOnce() {
      this.initPromise_ = new PromiseResolver();
      this.initialized_ = false;
      return this.initPromise_.promise;
    }

    /**
     * Track actions called |name|, allowing that type of action to be waited
     * for with `waitForAction`.
     * @param {string} name
     */
    expectAction(name) {
      this.resolverMap_.set(name, new PromiseResolver());
    }

    /**
     * Returns a Promise that will resolve when an action called |name| is
     * dispatched. The promise must be prepared by calling
     * `expectAction(name)` before the action is dispatched.
     * @param {string} name
     * @return {!Promise<!Action>}
     */
    async waitForAction(name) {
      assertTrue(
          this.resolverMap_.has(name),
          'Must call expectAction before each call to waitForAction');
      const action = await this.resolverMap_.get(name).promise;
      this.resolverMap_.delete(name);
      return action;
    }
  }

  // #cr_define_end
  return {
    TestStore: TestStore,
  };
});
