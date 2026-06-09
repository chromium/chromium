// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActorTaskInterruptReason, ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, CancelActionsResult} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertRejects, assertTrue, checkDefined, longWaitTimeMs, observeSequence, testMain} from './browser_test_base.js';

class GlicActorTaskLifecycleFunctionalBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async getFocusedTabId(): Promise<string> {
    assertDefined(this.host.getFocusedTabStateV2);
    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    return checkDefined(focus.hasFocus?.tabData.tabId);
  }

  base64ToArrayBuffer(base64: string): ArrayBuffer {
    return Uint8Array.fromBase64(base64).buffer as ArrayBuffer;
  }

  arrayBufferToBase64(buffer: ArrayBuffer): string {
    return (new Uint8Array(buffer)).toBase64();
  }

  /**
   * Waits for the actor task to reach the expected state.
   * Note: This cannot be used to wait for ActorTaskState.STOPPED because the
   * underlying observable is deleted immediately when the task stops, which
   * can cause race conditions and hang the test.
   */
  async waitForTaskState(taskId: number, expectedState: ActorTaskState):
      Promise<ActorTaskState> {
    assertTrue(
        expectedState !== ActorTaskState.STOPPED,
        'waitForTaskState cannot reliably wait for STOPPED state because the ' +
            'observable is deleted upon stop.');
    assertDefined(this.host.getActorTaskState);
    const seq = observeSequence(this.host.getActorTaskState(taskId));
    const result = await seq.waitForValue(expectedState);
    seq.unsubscribe();
    return result;
  }

  async testPauseAndResumeCreatedTask() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.pauseActorTask);
    assertDefined(this.host.resumeActorTask);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    const focusedTabId = await this.getFocusedTabId();

    await this.host.pauseActorTask(
        taskId, ActorTaskPauseReason.PAUSED_BY_USER, focusedTabId);
    await this.waitForTaskState(taskId, ActorTaskState.PAUSED);
    await this.advanceToNextStep({taskId});

    // Performing an action on a paused task should fail.
    const targetUrl = this.getUrl('/actor/blank.html?target');
    const navBuffer = await this.browser.makeNavigateAction(taskId, targetUrl);
    const performResult1 = await this.host.performActions(navBuffer);
    const resultCode1 = await this.browser.parseActionsResult(performResult1);
    assertEquals('kTaskPaused', resultCode1);

    const resumeResult = await this.host.resumeActorTask(taskId, {});
    assertEquals(0, resumeResult.actionResult);

    // Performing an action on a resumed task should succeed.
    const performResult2 = await this.host.performActions(navBuffer);
    const resultCode2 = await this.browser.parseActionsResult(performResult2);
    assertEquals('kOk', resultCode2);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testPauseAndResumeInvalidTask() {
    assertDefined(this.host.pauseActorTask);
    assertDefined(this.host.resumeActorTask);

    const focusedTabId = await this.getFocusedTabId();

    await this.host.pauseActorTask(
        12345, ActorTaskPauseReason.PAUSED_BY_USER, focusedTabId);
    await this.advanceToNextStep();

    await assertRejects(this.host.resumeActorTask(12345, {}));
  }

  async testPauseAndResumeInactiveTask() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.stopActorTask);
    assertDefined(this.host.resumeActorTask);
    assertDefined(this.host.pauseActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
    await this.advanceToNextStep({taskId});

    const focusedTabId = await this.getFocusedTabId();

    await this.host.pauseActorTask(
        taskId, ActorTaskPauseReason.PAUSED_BY_USER, focusedTabId);
    await this.advanceToNextStep();

    await assertRejects(this.host.resumeActorTask(taskId, {}));
  }

  async testPauseActiveTask() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.pauseActorTask);
    assertDefined(this.host.resumeActorTask);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.advanceToNextStep({taskId});

    // Use a long wait to ensure we can pause before it completes.
    const waitBuffer =
        await this.browser.makeWaitAction(taskId, longWaitTimeMs);
    const targetUrl = this.getUrl('/actor/blank.html?target');

    const focusedTabId = await this.getFocusedTabId();

    const waitPromise = this.host.performActions(waitBuffer);
    await this.host.pauseActorTask(
        taskId, ActorTaskPauseReason.PAUSED_BY_USER, focusedTabId);
    await this.waitForTaskState(taskId, ActorTaskState.PAUSED);

    const waitResult = await waitPromise;
    const resultCode1 = await this.browser.parseActionsResult(waitResult);
    assertEquals('kTaskPaused', resultCode1);

    const resumeResult = await this.host.resumeActorTask(taskId, {});
    assertEquals(0, resumeResult.actionResult);

    // Verify new Actions can be performed after the task is resumed.
    const navBuffer = await this.browser.makeNavigateAction(taskId, targetUrl);
    const navResult = await this.host.performActions(navBuffer);
    const resultCode2 = await this.browser.parseActionsResult(navResult);
    assertEquals('kOk', resultCode2);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testStopActiveTaskWithModelError() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.advanceToNextStep({taskId});

    const waitBuffer =
        await this.browser.makeWaitAction(taskId, longWaitTimeMs);
    const waitPromise = this.host.performActions(waitBuffer);

    // Wait for the task to start acting before stopping.
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.MODEL_ERROR);

    const waitResult = await waitPromise;
    const resultCode = await this.browser.parseActionsResult(waitResult);
    assertEquals('kTaskWentAway', resultCode);
  }

  async testInterruptAndUninterruptInvalidTask() {
    assertDefined(this.host.interruptActorTask);
    assertDefined(this.host.uninterruptActorTask);

    await this.host.interruptActorTask(12345);
    await this.advanceToNextStep();

    await this.host.uninterruptActorTask(12345);
  }

  async testInterruptAndUninterruptTaskWithCompletedActions() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.interruptActorTask);
    assertDefined(this.host.uninterruptActorTask);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.advanceToNextStep({taskId});

    const targetUrl = this.getUrl('/actor/blank.html?target');
    const navBuffer = await this.browser.makeNavigateAction(taskId, targetUrl);
    const navResult = await this.host.performActions(navBuffer);
    const resultCode = await this.browser.parseActionsResult(navResult);
    assertEquals('kOk', resultCode);

    await this.host.interruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.IDLE);

    await this.host.uninterruptActorTask(taskId);
    // Ensure uninterrupting a task with no pending actions keeps the state as
    // IDLE.
    await this.waitForTaskState(taskId, ActorTaskState.IDLE);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testInterruptAndUninterruptActiveTaskAndPerformActions() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.interruptActorTask);
    assertDefined(this.host.uninterruptActorTask);
    assertDefined(this.host.cancelActions);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.advanceToNextStep({taskId});

    const waitBuffer =
        await this.browser.makeWaitAction(taskId, longWaitTimeMs);
    const waitPromise = this.host.performActions(waitBuffer);

    // Wait for the task to start acting before interrupting.
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    await this.host.interruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.IDLE);

    // Ensure uninterrupting a task with previously pending actions sets the
    // state to ACTING.
    await this.host.uninterruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    // Since the ongoing long wait action must be completed before sending
    // another async action, we need to use the CancelActions API to cancell all
    // the ongoing actions on the task.
    const cancelResult = await this.host.cancelActions(taskId);
    assertEquals(CancelActionsResult.SUCCESS, cancelResult);
    const waitResult = await waitPromise;
    const resultCode1 = await this.browser.parseActionsResult(waitResult);
    assertEquals('kActionsCancelled', resultCode1);

    // Ensure the task can still perform actions after being uninterrupted.
    const targetUrl = this.getUrl('/actor/blank.html?target');
    const navBuffer = await this.browser.makeNavigateAction(taskId, targetUrl);
    const navResult = await this.host.performActions(navBuffer);
    const resultCode2 = await this.browser.parseActionsResult(navResult);
    assertEquals('kOk', resultCode2);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testInterruptWithReasons() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.interruptActorTask);
    assertDefined(this.host.uninterruptActorTask);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.advanceToNextStep({taskId});

    // Use a long wait to ensure we can interrupt before it completes.
    const waitBuffer =
        await this.browser.makeWaitAction(taskId, longWaitTimeMs);
    this.host.performActions(waitBuffer);

    // Wait for the task to start acting before interrupting.
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    // Test interrupting with no reason.
    await this.host.interruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.IDLE);
    await this.host.uninterruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    // Test interrupting with a specific reason.
    await this.host.interruptActorTask(
        taskId, ActorTaskInterruptReason.WAITING_USER_INPUT);
    await this.waitForTaskState(taskId, ActorTaskState.IDLE);
    await this.host.uninterruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    // Test interrupting with another specific reason.
    await this.host.interruptActorTask(
        taskId, ActorTaskInterruptReason.WAITING_USER_CONFIRMATION);
    await this.waitForTaskState(taskId, ActorTaskState.IDLE);
    await this.host.uninterruptActorTask(taskId);
    await this.waitForTaskState(taskId, ActorTaskState.ACTING);

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testActuatingChangedCallback() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    await this.advanceToNextStep({taskId});

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testActivateTabWithConversationUsesActorState() {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.stopActorTask);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    const firstTabId = await this.getFocusedTabId();
    // Let the test create a second tab.
    await this.advanceToNextStep();

    const targetUrl = this.getUrl('/actor/blank.html?target');
    const navBuffer =
        await this.browser.makeNavigateAction(taskId, targetUrl, firstTabId);
    const navResult = await this.host.performActions(navBuffer);
    const resultCode = await this.browser.parseActionsResult(navResult);
    assertEquals('kOk', resultCode);
  }
}

testMain([
  GlicActorTaskLifecycleFunctionalBrowserTest,
]);
