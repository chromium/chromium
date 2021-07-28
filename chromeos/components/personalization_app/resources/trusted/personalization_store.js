// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Store, StoreObserver} from 'chrome://resources/js/cr/ui/store.m.js';
import {StoreClient, StoreClientInterface} from 'chrome://resources/js/cr/ui/store_client.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {emptyState, PersonalizationState, reduce} from './personalization_reducers.js';

/**
 * @fileoverview A singleton datastore for the personalization app. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store. Any data that is shared between components should go here.
 */

/** @extends {Store<PersonalizationState>} */
export class PersonalizationStore extends Store {
  constructor() {
    super(emptyState(), reduce);
  }

  /** @return {!PersonalizationStore} */
  static getInstance() {
    return instance_ || (instance_ = new PersonalizationStore());
  }

  /** @param {?PersonalizationStore} instance */
  static setInstance(instance) {
    instance_ = instance;
  }
}

/**
 * @type {?PersonalizationStore}
 * @private
 */
let instance_ = null;

/**
 * @polymerBehavior
 */
const PersonalizationStoreClientImpl = {
  /**
   * Override onStateChanged so property updates are batched. Reduces churn
   * in polymer components when multiple state values change.
   * @override
   * @param {!PersonalizationState} newState
   */
  onStateChanged(newState) {
    const changes = this.watches_.reduce((result, watch) => {
      const oldValue = this[watch.localProperty];
      const newValue = watch.valueGetter(newState);
      if (newValue !== oldValue && newValue !== undefined) {
        result[watch.localProperty] = newValue;
      }
      return result;
    }, {});
    this.setProperties(changes);
  },

  /**
   * @param {string} localProperty
   * @param {function(!PersonalizationState):*} valueGetter
   */
  watch(localProperty, valueGetter) {
    this.watch_(localProperty, valueGetter);
  },
  /** @return {!PersonalizationState} */
  getState() {
    return this.getStore().data;
  },
  /** @return {!PersonalizationStore} */
  getStore() {
    return PersonalizationStore.getInstance();
  },
}

export class PersonalizationStoreClientInterface {
  /**
   * @param {string} localProperty
   * @param {function(!PersonalizationState): *} valueGetter
   */
  watch(localProperty, valueGetter) {}

  /** @return {!PersonalizationState} */
  getState() {}

  /** @return {!PersonalizationStore} */
  getStore() {}
}

/**
 * @polymerBehavior
 * @implements {PersonalizationStoreClientInterface}
 * @implements {StoreClientInterface}
 * @implements {StoreObserver<PersonalizationState>}
 * @implements {I18nBehavior}
 */
export const PersonalizationStoreClient =
    [StoreClient, PersonalizationStoreClientImpl, I18nBehavior];

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {PersonalizationStoreClientInterface}
 * @implements {StoreClientInterface}
 * @implements {StoreObserver<PersonalizationState>}
 * @implements {I18nBehaviorInterface}
 */
export const WithPersonalizationStore =
    mixinBehaviors(PersonalizationStoreClient, PolymerElement);
