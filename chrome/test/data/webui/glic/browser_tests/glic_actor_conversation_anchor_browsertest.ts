// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path:
// chrome/browser/glic/actor/glic_actor_conversation_anchor_browsertest.cc

import {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason} from '/glic/glic_api/glic_api.js';
import type {NavigationConfirmationRequest, PerformActionsOptions} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertTrue, checkDefined, observeSequence, testMain} from './browser_test_base.js';

class GlicActorConversationAnchorBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
    this.host.selectNavigationConfirmationRequestHandler?.()?.subscribe(
        (request: NavigationConfirmationRequest) => {
          request.onConfirmationDecision({
            response: {permissionGranted: true},
          });
        });
  }

  async getFocusedTabId(): Promise<string> {
    assertDefined(this.host.getFocusedTabStateV2);
    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    return checkDefined(focus.hasFocus?.tabData.tabId);
  }

  async createTaskAndNavigateAction():
      Promise<{taskId: number, navigateAction: ArrayBuffer}> {
    assertDefined(this.host.createTask);
    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);
    const navigateAction = await this.browser.makeNavigateAction(
        taskId, this.getHttpsUrl('/actor/blank.html?target'));
    return {taskId, navigateAction};
  }

  // Returns the result code of a performActions() call.
  async performActions(actions: ArrayBuffer, options?: PerformActionsOptions):
      Promise<string> {
    assertDefined(this.host.performActions);
    const result = options === undefined ?
        await this.host.performActions(actions) :
        await this.host.performActions(actions, options);
    return this.browser.parseActionsResult(result);
  }

  async testTurnIdSetsConversationAnchor() {
    assertDefined(this.host.stopActorTask);
    const {taskId, navigateAction} = await this.createTaskAndNavigateAction();

    assertEquals(
        'kOk', await this.performActions(navigateAction, {turnId: 'r_turn_1'}));
    // Calls without a turn ID clear the anchor.
    assertEquals('kOk', await this.performActions(navigateAction));

    await this.advanceToNextStep({taskId});
    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testRejectedCallDoesNotSetConversationAnchor() {
    assertDefined(this.host.pauseActorTask);
    assertDefined(this.host.resumeActorTask);
    assertDefined(this.host.getActorTaskState);
    assertDefined(this.host.stopActorTask);
    const {taskId, navigateAction} = await this.createTaskAndNavigateAction();

    await this.host.pauseActorTask(
        taskId, ActorTaskPauseReason.PAUSED_BY_USER,
        await this.getFocusedTabId());
    const taskState = observeSequence(this.host.getActorTaskState(taskId));
    await taskState.waitForValue(ActorTaskState.PAUSED);
    taskState.unsubscribe();

    // The actor rejects actions for a paused task.
    assertEquals(
        'kTaskPaused',
        await this.performActions(navigateAction, {turnId: 'r_paused'}));

    const resumeResult = await this.host.resumeActorTask(taskId, {});
    assertEquals(0, resumeResult.actionResult);
    assertEquals(
        'kOk',
        await this.performActions(navigateAction, {turnId: 'r_resumed'}));

    await this.advanceToNextStep({taskId});
    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }
}

testMain([
  GlicActorConversationAnchorBrowserTest,
]);
