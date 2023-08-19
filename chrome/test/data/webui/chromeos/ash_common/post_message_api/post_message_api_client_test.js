// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';
import {RequestHandler} from 'chrome://resources/ash/common/post_message_api/post_message_api_request_handler.js';

const ServerOriginURLFilter = 'chrome://chrome-signin/';

class TestRequestHandler extends RequestHandler {
  constructor() {
    super(null, ServerOriginURLFilter, ServerOriginURLFilter);
    this.registerMethod('isTestFinalized', this.isTestFinalized_.bind(this));
    this.registerMethod(
        'rejectedPromiseTest', this.rejectedPromiseTest_.bind(this));
    this.isTestFinalized_ = false;
  }

  /** @override */
  targetWindow() {
    return window.parent;
  }

  // Called by client when the test cases are satisfied.
  onTestFinalized() {
    this.isTestFinalized_ = true;
  }

  /**
   * PostMessageAPIRequest that comes from the server to check if test is
   * finalized.
   * @return {boolean}
   */
  isTestFinalized_() {
    return this.isTestFinalized_;
  }

  /**
   * A test used to ensure that rejected promises are passed to client.
   * @return {Promise<boolean>}
   */
  rejectedPromiseTest_(args) {
    const reject = args[0];
    if (reject) {
      return Promise.reject(new Error('Promise rejected'));
    }
    return Promise.resolve(true);
  }
}

class TestPostMessageApiClient extends PostMessageApiClient {
  constructor(requestHandler) {
    super(ServerOriginURLFilter, null);
    this.requestHandler_ = requestHandler;
  }

  setX(x) {
    return this.callApiFn('setX', x);
  }

  increment(y) {
    return this.callApiFn('increment', y);
  }

  decrement(z) {
    return this.callApiFn('decrement', z);
  }

  finalize(success) {
    return this.callApiFn('finalize', success);
  }

  /**
   * Exercise the methods in the METHOD list above, which are defined in the
   * TestPostMessageAPIServer class.
   * @override
   */
  onInitialized() {
    // Iinitializes the "x" value in the test PostMessageAPIServer and ensures
    // that it has been set via the callback.
    this.setX(1).then((result) => {
      if (result !== 1) {
        this.finalize(false);
        return;
      }

      // Increments the "x" value in the test PostMessageAPIServer and ensures
      // that the value has been updated.
      this.increment(5).then((result) => {
        if (result !== 6) {
          this.finalize(false);
          return;
        }

        // Decrements the "x" value in the test PostMessageAPIServer and ensures
        // that the value has been updated.
        this.decrement(1).then((result) => {
          if (result !== 5) {
            this.finalize(false);
            return;
          }

          // By this time, multiple requests have been successfully sent and
          // received between the test PostMessageAPIServer and
          // PostMessageApiClient. Notify the server that the test is
          // successfully completed.
          this.finalize(true);
          this.requestHandler_.onTestFinalized();
        });
      });
    });
  }
}

document.addEventListener('DOMContentLoaded', function() {
  // Construct the PostMessageApiClient so that it can run the tests.
  const postMessageClient =
      new TestPostMessageApiClient(new TestRequestHandler());
});
