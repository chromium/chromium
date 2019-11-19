// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Host-side of web-driver like controller for sandboxed guest frames. */
class GuestDriver {
  /** @param {string} origin */
  constructor(origin) {
    this.origin = origin;

    /** @type {Array<MessageEvent<TestMessageResponseData>>} */
    this.testMessageQueue = [];

    /** @type {?function(MessageEvent<TestMessageResponseData>)} */
    this.testMessageWaiter = null;

    this.messageListener = (/** Event */ event) => {
      const testEvent =
          /** @type{MessageEvent<TestMessageResponseData>} */ (event);

      console.log('Event from guest: ' + JSON.stringify(testEvent.data));
      if (this.testMessageWaiter) {
        this.testMessageWaiter(testEvent);
        this.testMessageWaiter = null;
      } else {
        this.testMessageQueue.push(testEvent);
      }
    };

    window.addEventListener('message', this.messageListener);
  }

  tearDown() {
    window.removeEventListener('message', this.messageListener);
  }

  /**
   * Returns the next message from the guest.
   * @return {Promise<MessageEvent<TestMessageResponseData>>}
   */
  popTestMessage() {
    if (this.testMessageQueue.length > 0) {
      const front = this.testMessageQueue[0];
      this.testMessageQueue.shift();
      return Promise.resolve(front);
    }
    return new Promise(resolve => {
      this.testMessageWaiter = resolve;
    });
  }

  /**
   * Sends a query to the guest that repeatedly runs a query selector until
   * it returns an element.
   *
   * @param {string} query the querySelector to run in the guest.
   * @param {?string} opt_property a property to request on the found element.
   * @return Promise<string> JSON.stringify()'d value of the property, or
   *   tagName if unspecified.
   */
  async waitForElementInGuest(query, opt_property) {
    const frame = assertInstanceof(
        document.querySelector(`iframe[src^="${this.origin}"]`),
        HTMLIFrameElement);
    /** @type{TestMessageQueryData} */
    const message = {testQuery: query, property: opt_property};
    frame.contentWindow.postMessage(message, this.origin);
    const event = await this.popTestMessage();
    return event.data.testQueryResult;
  }
}
