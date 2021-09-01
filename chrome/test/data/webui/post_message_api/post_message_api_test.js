// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/js/post_message_api_client.m.js';
import {PostMessageAPIServer} from 'chrome://resources/js/post_message_api_server.m.js';

const TARGET_URL = 'chrome://test/post_message_api/iframe.html';
const TARGET_ORIGIN = 'chrome://test/';

class TestPostMessageAPIServer extends PostMessageAPIServer {
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
    const promise = new Promise((resolve, reject) => {
      this.promise_resolve = resolve;
    });

    if (this.success) {
      this.promise_resolve(this.success);
    }
    return promise;
  }
}

class TestClient extends PostMessageAPIClient {
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
}

suite('PostMessageAPIModuleTest', function() {
  suiteSetup(function() {
    this.innerFrame = document.createElement('iframe');
    this.innerFrame.src = TARGET_URL;
    document.body.appendChild(this.innerFrame);
  });

  test('PostMessageCommTest', async function() {
    var server = new TestPostMessageAPIServer(this.innerFrame);
    let success = await server.getTestFinalized();
    assertTrue(success);

    //  Bootstraps a duplex communication channel between this server and the
    //  client handler in the iframe.
    var client = new TestClient(this.innerFrame);
    success = await client.isTestFinalized();
    assertTrue(success);
  });
});
