// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExperimentalTriggeringUpdateType} from '/glic/glic_api/glic_api.js';
import type {ExperimentalTriggeringUpdate, Observable2} from '/glic/glic_api/glic_api.js';
import {Subject} from '/glic/observable.js';

import {ApiTestFixtureBase, runUntil, testMain, WebClient} from './browser_test_base.js';

class TriggeringUpdatesClient extends WebClient {
  triggeringUpdatesSubject = new Subject<ExperimentalTriggeringUpdate>();
  isSubscribed = false;

  async getExperimentalTriggeringUpdates():
      Promise<Observable2<ExperimentalTriggeringUpdate>> {
    this.isSubscribed = true;
    return this.triggeringUpdatesSubject;
  }
}

const client = new TriggeringUpdatesClient();

class TriggeringUpdatesTest extends ApiTestFixtureBase {
  override createWebClient() {
    return client;
  }

  async testGetExperimentalTriggeringUpdates() {
    // Push a terminal update to trigger completion.
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.TERMINAL_COMPLETION,
      data: '',
    });
    client.triggeringUpdatesSubject.complete();
  }

  async testRelaysUpdatesWithSequenceNumbers() {
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update_1',
    });
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update_2',
    });
  }

  async testRespectsLastSeenSequenceNumber() {
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update',
    });
  }

  async testHandlesStartAndStopActuationRequestsSuccessfully() {
    await runUntil(() => client.isSubscribed);
  }

  async testHandlesStopActuationRequestNoMatchingUpdatesHandler() {
    // No-op.
  }
}

testMain([TriggeringUpdatesTest]);
