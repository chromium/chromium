// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a mock store for personalization app. Use to monitor actions
 * and manipulate state.
 *
 * Skip type-checks for this file because ../../test_store.js is not properly
 * typed.
 * @suppress {checkTypes}
 */

import {reduce} from 'chrome://personalization/trusted/personalization_reducers.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {PersonalizationStore} from 'chrome://personalization/trusted/personalization_store.js';
import {TestStore} from '../../test_store.js';

export class TestPersonalizationStore extends TestStore {
  constructor(data) {
    super(data, PersonalizationStore, emptyState(), reduce);
    this.actions_ = [];
    this.states_ = [];
  }

  get actions() {
    return this.actions_;
  }

  get states() {
    return this.states_;
  }

  /** @override */
  replaceSingleton() {
    PersonalizationStore.setInstance(this);
  }

  /** @override */
  reduce_(action) {
    super.reduce_(action);
    this.actions_.push(action);
    this.states_.push(this.data);
  }

  reset(data = {}) {
    this.data = Object.assign(emptyState(), data);
    this.resetLastAction();
    this.actions_ = [];
    this.states_ = [];
  }
}
