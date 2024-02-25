// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a mock store for personalization app. Use to monitor actions
 * and manipulate state.
 */

import {Actions, emptyState, PersonalizationState, PersonalizationStore, reduce} from 'chrome://personalization/js/personalization_app.js';
import {TestStore} from 'chrome://webui-test/test_store.js';

/**
 * Records actions and states observed during a test run. A Personalization App
 * specific specialization of the generic TestStore.
 */
export class TestPersonalizationStore extends
    TestStore<PersonalizationState, Actions> {
  // received actions and states.
  private actions_: Actions[];
  private states_: any[];

  override data: PersonalizationState = emptyState();

  constructor(data: Partial<PersonalizationState>) {
    super(data, emptyState(), reduce);
    this.actions_ = [];
    this.states_ = [];
  }

  get actions() {
    return this.actions_;
  }

  get states() {
    return this.states_;
  }

  override reduce(action: Actions) {
    super.reduce(action);
    this.actions_.push(action);
    this.states_.push(this.data);
  }

  replaceSingleton() {
    PersonalizationStore.setInstance(this);
  }

  reset(data = {}) {
    this.data = Object.assign(emptyState(), data);
    this.resetLastAction();
    this.actions_ = [];
    this.states_ = [];
  }
}
