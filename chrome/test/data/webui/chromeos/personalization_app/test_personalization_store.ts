// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a mock store for personalization app. Use to monitor actions
 * and manipulate state.
 */

import {emptyState, PersonalizationStore, reduce} from 'chrome://personalization/trusted/personalization_app.js';
import {Action} from 'chrome://resources/js/cr/ui/store.js';
import {TestStore} from 'chrome://webui-test/test_store.js';

export class TestPersonalizationStore extends TestStore {
  // received actions and states.
  private actions_: Action[];
  private states_: any[];

  constructor(data: any) {
    super(data, PersonalizationStore, emptyState(), reduce);
    this.actions_ = [];
    this.states_ = [];

    // manually override `reduce_` method because it's private.
    this['reduce_'] = (action: Action) => {
      super['reduce_'](action);
      this.actions_.push(action);
      this.states_.push(this.data);
    };
  }

  get actions() {
    return this.actions_;
  }

  get states() {
    return this.states_;
  }

  override replaceSingleton() {
    PersonalizationStore.setInstance(this);
  }

  reset(data = {}) {
    this.data = Object.assign(emptyState(), data);
    this.resetLastAction();
    this.actions_ = [];
    this.states_ = [];
  }
}
