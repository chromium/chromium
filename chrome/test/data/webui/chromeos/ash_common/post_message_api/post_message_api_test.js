// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';
import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';

const TARGET_URL =
    'chrome://webui-test/chromeos/ash_common/post_message_api/iframe.html';
const TARGET_ORIGIN = 'chrome://webui-test/';

class TestPostMessageAPIServer extends PostMessageApiServer {
  constructor(iframeElement) {
    super(iframeElement, TARGET_URL, TARGET_ORIGIN);
    /**
     * Whether the test was successful or not.
     * {boolean}
     */
    this.success = false;

    /**
     * The value that is being communicated between the server and client.
     * {integer}
     */
    this.x = -1;

    /**
     * The promise to be resolved when test finishes successfully.
     * {Promise<boolean>}
     */
    this.promise_resolve = null;

    this.registerMethod('setX', this.setX.bind(this));
    this.registerMethod('increment', this.increment.bind(this));
    this.registerMethod('decrement', this.decrement.bind(this));
    this.registerMethod('finalize', this.finalize.bind(this));
  }

  /**
   * @param {int} x
   */
  setX(x) {
    this.x = x;
    return this.x;
  }

  /**
   * @param {int} y
   */
  increment(y) {
    this.x = this.x + y;
    return this.x;
  }
  /**
   * @param {int} y
   */
  decrement(y) {
    this.x = this.x - y;
    return this.x;
  }
  /**
   * @param {boolean} success
   */
  finalize(success) {
    this.success = success;
    if (this.promise_resolve) {
      this.promise_resolve(this.success);
    }
  }

  /**
   * Returns a promise which when resolved will tell whether the test passed or
   * not.
   * @return {Promise<boolean>}
   */
  getTestFinalized() {
    if (!this.success) {
      const promise = new Promise((resolve, reject) => {
        this.promise_resolve = resolve;
      });
      return promise;
    }
    return Promise.resolve(this.sucess);
  }
}

class TestClient extends PostMessageApiClient {
  constructor(iframeElement) {
    super(TARGET_ORIGIN, iframeElement.contentWindow);
  }

  /**
   * Sends request to the iframe element to check if test is finalized.
   * @return {Promise<boolean>}
   */
  isTestFinalized() {
    return this.callApiFn('isTestFinalized', null);
  }

  /**
   * Used to test that rejected promises are passed back to client and the
   * client handles them appropriately.
   * @param {boolean} reject. Whether this all should be rejected or not.
   * @return {Promise<boolean>}
   */
  rejectedPromiseTest(reject) {
    return this.callApiFn('rejectedPromiseTest', [reject]);
  }
}

suite('PostMessageAPIModuleTest', function() {
  suiteSetup(function() {
    this.innerFrame = document.createElement('iframe');
    this.innerFrame.src = TARGET_URL;
    document.body.appendChild(this.innerFrame);
  });

  test('PostMessageCommTest', async function() {
    const server = new TestPostMessageAPIServer(this.innerFrame);
    let success = await server.getTestFinalized();
    assertTrue(success);

    //  Bootstraps a duplex communication channel between this server and the
    //  client handler in the iframe.
    const client = new TestClient(this.innerFrame);

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
