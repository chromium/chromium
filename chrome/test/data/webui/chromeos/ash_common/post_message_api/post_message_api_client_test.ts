// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';
import {RequestHandler} from 'chrome://resources/ash/common/post_message_api/post_message_api_request_handler.js';

const SERVER_ORIGIN_URL_FILTER = 'chrome://chrome-signin/';

class TestRequestHandler extends RequestHandler {
  testFinalized: boolean = false;

  constructor() {
    super(
        window.frameElement!, SERVER_ORIGIN_URL_FILTER,
        SERVER_ORIGIN_URL_FILTER);
    this.registerMethod('isTestFinalized', this.isTestFinalized.bind(this));
    this.registerMethod(
        'rejectedPromiseTest', this.rejectedPromiseTest.bind(this));
  }

  override targetWindow() {
    return window.parent;
  }

  /** Called by client when the test cases are satisfied. */
  onTestFinalized() {
    this.testFinalized = true;
  }

  /**
   * PostMessageAPIRequest that comes from the server to check if test is
   * finalized.
   */
  private isTestFinalized(): boolean {
    return this.testFinalized;
  }

  /** A test used to ensure that rejected promises are passed to client. */
  private rejectedPromiseTest(args: any[]): Promise<boolean> {
    const reject = args[0];
    if (reject) {
      return Promise.reject(new Error('Promise rejected'));
    }
    return Promise.resolve(true);
  }
}

class TestPostMessageApiClient extends PostMessageApiClient {
  private requestHandler: TestRequestHandler;

  constructor(requestHandler: TestRequestHandler) {
    super(SERVER_ORIGIN_URL_FILTER, null);
    this.requestHandler = requestHandler;
  }

  setMessageValue(value: number) {
    return this.callApiFn('setMessageValue', [value]);
  }

  incrementMessageValue(value: number) {
    return this.callApiFn('incrementMessageValue', [value]);
  }

  decrementMessageValue(value: number) {
    return this.callApiFn('decrementMessageValue', [value]);
  }

  finalize(success: boolean) {
    return this.callApiFn('finalize', [success]);
  }

  /**
   * Exercise the methods in the METHOD list above, which are defined in the
   * TestPostMessageAPIServer class.
   */
  override onInitialized() {
    // Iinitializes the message value in the TestPostMessageAPIServer and
    // ensures that it has been set via the callback.
    this.setMessageValue(1).then((result) => {
      if (result !== 1) {
        this.finalize(false);
        return;
      }

      // Increments the message value in the TestPostMessageAPIServer and
      // ensures that the value has been updated.
      this.incrementMessageValue(5).then((result) => {
        if (result !== 6) {
          this.finalize(false);
          return;
        }

        // Decrements the message value in the TestPostMessageAPIServer and
        // ensures that the value has been updated.
        this.decrementMessageValue(1).then((result) => {
          if (result !== 5) {
            this.finalize(false);
            return;
          }

          // By this time, multiple requests have been successfully sent and
          // received between the TestPostMessageAPIServer and
          // TestPostMessageApiClient. Notify the server that the test is
          // successfully completed.
          this.finalize(true);
          this.requestHandler.onTestFinalized();
        });
      });
    });
  }
}

document.addEventListener('DOMContentLoaded', function() {
  // Construct the PostMessageApiClient so that it can run the tests.
  new TestPostMessageApiClient(new TestRequestHandler());
});
