// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a mock store for personalization app. Use to monitor actions
 * and manipulate state.
 */

import {emptyState, PersonalizationState, PersonalizationStore, reduce} from 'chrome://personalization/js/personalization_app.js';
import {Action} from 'chrome://resources/ash/common/store/store.js';
import {TestStore} from 'chrome://webui-test/chromeos/test_store.js';

export class TestPersonalizationStore extends TestStore {
  // received actions and states.
  private actions_: Action[];
  private states_: any[];

  override data: PersonalizationState = emptyState();

  constructor(data: any) {
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

  override reduce(action: Action) {
    super.reduce(action);
    this.actions_.push(action);
    this.states_.push(this.data);
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
