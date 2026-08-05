// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActuationTarget, ExperimentalTriggeringUpdateType} from '/glic/glic_api/glic_api.js';
import type {ExperimentalTriggeringUpdate, Observable2} from '/glic/glic_api/glic_api.js';
import {Subject} from '/glic/observable.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, runUntil, testMain, WebClient} from './browser_test_base.js';

class TriggeringUpdatesClient extends WebClient {
  triggeringUpdatesSubject = new Subject<ExperimentalTriggeringUpdate>();
  isSubscribed = false;
  override mockFileToken = 'mock_file_token_from_client';

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
    await runUntil(() => client.isSubscribed);
    const invokeOpts = await client.invokeData.waitUntil(v => v !== undefined);
    assertDefined(invokeOpts);
    assertEquals(ActuationTarget.TARGET_SURFACE, invokeOpts.actuationTarget);
    // Push a terminal update to trigger completion.
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

  async testRelaysResumedUpdate(): Promise<void> {
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.RESUMED,
      data: '',
    });
  }

  async testRelaysConversationId() {
    await runUntil(() => client.isSubscribed);
    assertDefined(this.host.registerConversation);
    await this.host.registerConversation({
      conversationId: 'test_conv_id',
      conversationTitle: 'test',
    });
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update',
    });
  }

  async testHandlesStartAndStopActuationRequestsSuccessfully() {
    await runUntil(() => client.isSubscribed);
  }

  async testHandlesContinueActuationRequestSuccessfully() {
    await runUntil(() => client.isSubscribed);
  }

  async testContinueActuationTargetsCorrectTab() {
    await runUntil(() => client.isSubscribed);
    assertDefined(this.host.registerConversation);
    await this.host.registerConversation({
      conversationId: 'test_conv_id',
      conversationTitle: 'test',
    });
    assertDefined(this.host.createTask);
    await this.host.createTask();
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update',
    });
  }

  async testRelaysParentConversationMetadataUpdated() {
    await runUntil(() => client.isSubscribed);
    const contextPromise = new Promise<any>(resolve => {
      this.host.getAdditionalContext!().subscribe(context => {
        resolve(context);
      });
    });
    await this.advanceToNextStep();
    const context = await contextPromise;
    assertDefined(context);
    assertEquals(1, context.parts.length);
    const metadata = context.parts[0]!.parentConversationMetadata;
    assertDefined(metadata);
    assertEquals('test_conv_id', metadata.conversationId);
    assertEquals('test_title', metadata.conversationTitle);
  }

  async testRelaysParentConversationMetadataInitial() {
    await runUntil(() => client.isSubscribed);
    const invokeOpts = await client.invokeData.waitUntil(v => v !== undefined);
    assertDefined(invokeOpts);
    assertEquals(ActuationTarget.TARGET_SURFACE, invokeOpts.actuationTarget);
    assertDefined(invokeOpts.context);
    assertEquals(1, invokeOpts.context!.parts.length);
    const metadata = invokeOpts.context!.parts[0]!.parentConversationMetadata;
    assertDefined(metadata);
    assertEquals('test_init_id', metadata.conversationId);
    assertEquals('test_init_title', metadata.conversationTitle);
  }

  async testHandlesGetScreenshotRequestSuccessfully() {
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'ready_for_screenshot',
    });
  }

  async testRelaysUpdatesWithMetadataEnabled() {
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update_with_metadata',
      metadata: {
        'key1': 'value1',
        'key2': 'value2',
      },
    });
  }

  async testRelaysUpdatesWithMetadataDisabled() {
    await runUntil(() => client.isSubscribed);
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.WORKLOG,
      data: 'test_update_with_metadata',
      metadata: {
        'key1': 'value1',
        'key2': 'value2',
      },
    });
  }
}

testMain([TriggeringUpdatesTest]);
