// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';
import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

const TARGET_URL =
    'chrome://webui-test/chromeos/ash_common/post_message_api/iframe.html';
const TARGET_ORIGIN = 'chrome://webui-test/';

class TestPostMessageApiServer extends PostMessageApiServer {
  /** Function that is executed when the test finishes successfully. */
  onSuccess: ((success: boolean) => void)|undefined;
  private success: boolean = false;
  /** The value that is being communicated between the server and client. */
  private messageValue: number = -1;

  constructor(iframeElement: HTMLIFrameElement) {
    super(iframeElement, TARGET_URL, TARGET_ORIGIN);

    this.registerMethod(
        'setMessageValue', (args) => this.setMessageValue(args));
    this.registerMethod(
        'incrementMessageValue', (args) => this.incrementMessageValue(args));
    this.registerMethod(
        'decrementMessageValue', (args) => this.decrementMessageValue(args));
    this.registerMethod('finalize', (args) => this.finalize(args));
  }

  setMessageValue(args: number[]): number {
    assertTrue(args[0] !== undefined);
    this.messageValue = args[0];
    return this.messageValue;
  }

  incrementMessageValue(args: number[]): number {
    assertTrue(args[0] !== undefined);
    this.messageValue = this.messageValue + args[0];
    return this.messageValue;
  }

  decrementMessageValue(args: number[]): number {
    assertTrue(args[0] !== undefined);
    this.messageValue = this.messageValue - args[0];
    return this.messageValue;
  }

  finalize(args: boolean[]) {
    assertTrue(args[0] !== undefined);
    this.success = args[0];
    if (this.onSuccess) {
      this.onSuccess(this.success);
    }
  }

  /**
   * Returns a promise which when resolved will tell whether the test passed or
   * not.
   */
  getTestFinalized(): Promise<any> {
    if (!this.success) {
      const promise = new Promise((resolve) => {
        this.onSuccess = resolve;
      });
      return promise;
    }
    return Promise.resolve(this.success);
  }
}

class TestClient extends PostMessageApiClient {
  constructor(iframeElement: HTMLIFrameElement) {
    super(TARGET_ORIGIN, iframeElement.contentWindow);
  }

  /** Sends request to the iframe element to check if test is finalized. */
  isTestFinalized() {
    return this.callApiFn('isTestFinalized', []);
  }

  /**
   * Used to test that rejected promises are passed back to client and the
   * client handles them appropriately.
   */
  rejectedPromiseTest(reject: boolean) {
    return this.callApiFn('rejectedPromiseTest', [reject]);
  }
}

suite('PostMessageApiModuleTest', function() {
  let innerFrame: HTMLIFrameElement;

  suiteSetup(function() {
    innerFrame = document.createElement('iframe') as HTMLIFrameElement;
    innerFrame.src = TARGET_URL;
    document.body.appendChild(innerFrame);
  });

  test('PostMessageCommTest', async function() {
    const server = new TestPostMessageApiServer(innerFrame);
    let success = await server.getTestFinalized();
    assertTrue(success);

    //  Bootstraps a duplex communication channel between this server and the
    //  client handler in the iframe.
    const client = new TestClient(innerFrame);

    // Test non-rejected request.
    success = await client.rejectedPromiseTest(/*reject=*/ false);
    assertTrue(success);

    // Test rejected test case.
    let rejected = false;
    try {
      await client.rejectedPromiseTest(/*reject=*/ true);
      rejected = false;
    } catch (error) {
      rejected = true;
    }

    // Assert that the request was rejected.
    assertTrue(rejected);

    success = await client.isTestFinalized();
    assertTrue(success);
  });
});
