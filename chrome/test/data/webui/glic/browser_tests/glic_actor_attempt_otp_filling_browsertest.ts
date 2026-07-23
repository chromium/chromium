// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActorTaskStopReason} from '/glic/glic_api/glic_api.js';
import type {GmailOtpOptInRequest} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertTrue, testMain} from './browser_test_base.js';

class GlicActorAttemptOtpFillingBrowserTest extends ApiTestFixtureBase {
  override async setUpTest(): Promise<void> {
    await this.client.waitForFirstOpen();
  }

  // Common logic to run the OTP filling flow.
  // The C++ side must pass the target element details in `testParams`.
  async runOtpFillingFlow(options: {
    acceptOptIn: boolean,
    expectSuccess: boolean,
    expectedResultCode: string,
  }): Promise<void> {
    assertDefined(this.host.createTask);
    assertDefined(this.host.performActions);
    assertDefined(this.host.stopActorTask);
    assertDefined(this.host.selectGmailOtpOptInRequestHandler);

    const taskId = await this.host.createTask();
    assertTrue(taskId > 0);

    // Subscribe to the opt-in request.
    const subscriber = this.host.selectGmailOtpOptInRequestHandler();
    assertDefined(subscriber);
    const dialogRequestPromise =
        new Promise<GmailOtpOptInRequest>((resolve) => {
          const subscription =
              subscriber.subscribe((request: GmailOtpOptInRequest) => {
                subscription.unsubscribe();
                resolve(request);
              });
        });

    // 1. Yield to C++ to navigate the tab and extract APC to find the OTP
    // field. C++ will continue us with { nodeId, documentIdentifier } in
    // testParams.
    await this.advanceToNextStep({taskId});

    assertDefined(this.testParams);
    const nodeId = this.testParams.nodeId;
    const documentIdentifier = this.testParams.documentIdentifier;
    assertTrue(nodeId > 0);
    assertTrue(documentIdentifier.length > 0);

    // Prepare the action buffer via C++ bridge.
    // Predicted OTP type 2 is EMAIL.
    const actionBuffer = await this.browser.makeAttemptOtpFillingAction(
        taskId, nodeId, documentIdentifier, /*forSignin=*/ true,
        /*otpType=*/ 2);

    // Start performing actions. This will trigger the opt-in dialog validation.
    const performPromise = this.host.performActions(actionBuffer);

    // Wait for the opt-in dialog request and respond.
    const request = await dialogRequestPromise;
    request.onDialogClosed({permissionGranted: options.acceptOptIn});

    // Wait for performActions to complete.
    const resultBuffer = await performPromise;
    const resultCode = await this.browser.parseActionsResult(resultBuffer);
    assertEquals(options.expectedResultCode, resultCode);

    if (options.expectSuccess) {
      // 2. Yield to C++ to verify that the field was filled.
      await this.advanceToNextStep();
    }

    await this.host.stopActorTask(taskId, ActorTaskStopReason.TASK_COMPLETE);
  }

  async testOptInDeclined(): Promise<void> {
    await this.runOtpFillingFlow({
      acceptOptIn: false,
      expectSuccess: false,
      expectedResultCode: 'kOtpUserDeclinedOptingIntoFilling',
    });
  }

  async testOptInAccepted(): Promise<void> {
    await this.runOtpFillingFlow({
      acceptOptIn: true,
      expectSuccess: true,
      expectedResultCode: 'kOk',
    });
  }

  async testOptInAcceptedButRetrievalFails(): Promise<void> {
    // This will be configured by C++ mocking the service to fail.
    await this.runOtpFillingFlow({
      acceptOptIn: true,
      expectSuccess: false,
      expectedResultCode: 'kOtpRetrievalError',
    });
  }
}

testMain([
  GlicActorAttemptOtpFillingBrowserTest,
]);
