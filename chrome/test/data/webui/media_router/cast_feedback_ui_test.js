// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FeedbackEvent, FeedbackUiBrowserProxy, FeedbackUiBrowserProxyImpl} from 'chrome://cast-feedback/cast_feedback_ui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';

/** @implements {FeedbackUiBrowserProxy} */
class TestFeedbackUiBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['recordEvent', 'sendFeedback']);

    /** @type {number} */
    this.timesToFail = 0;

    /** @const {!PromiseResolver} */
    this.resolver = new PromiseResolver();
  }

  /** @override */
  recordEvent(event) {
    this.methodCalled('recordEvent', event);
    if (event === FeedbackEvent.SUCCEEDED || event === FeedbackEvent.FAILED) {
      this.resolver.resolve();
    }
  }

  /** @override */
  sendFeedback(info) {
    this.methodCalled('sendFeedback', info);
    return Promise.resolve(
        this.getCallCount('sendFeedback') > this.timesToFail ?
            chrome.feedbackPrivate.Status.SUCCESS :
            chrome.feedbackPrivate.Status.DELAYED);
  }
}

suite('Suite', function() {
  let browserProxy;
  let ui;
  const TEST_COMMENT = 'test comment';

  function submit() {
    const textArea = ui.shadowRoot.querySelector('textarea');
    textArea.value = TEST_COMMENT;
    textArea.dispatchEvent(new CustomEvent('input'));

    const submitButton = ui.shadowRoot.querySelector('.action-button');
    submitButton.click();
  }

  setup(function() {
    browserProxy = new TestFeedbackUiBrowserProxy();
    FeedbackUiBrowserProxyImpl.instance_ = browserProxy;
    ui = document.createElement('feedback-ui');
    document.body.innerHtml = '';
    document.body.appendChild(ui);
  });

  test('Success', async function() {
    browserProxy.timesToFail = 1;
    ui.resendDelayMs = 50;
    ui.maxResendAttempts = 2;
    submit();
    await browserProxy.resolver.promise;
    assertEquals(2, browserProxy.getCallCount('sendFeedback'));
    assertDeepEquals(
        [
          FeedbackEvent.OPENED,
          FeedbackEvent.SENDING,
          FeedbackEvent.RESENDING,
          FeedbackEvent.SUCCEEDED,
        ],
        browserProxy.getArgs('recordEvent'));
    assertTrue(ui.feedbackSent);
    assertTrue(
        browserProxy.getArgs('sendFeedback')[0].description.indexOf(
            TEST_COMMENT) !== -1);
  });


  test('Failure', async function() {
    browserProxy.timesToFail = 3;
    ui.resendDelayMs = 50;
    ui.maxResendAttempts = 2;
    submit();
    await browserProxy.resolver.promise;
    assertEquals(3, browserProxy.getCallCount('sendFeedback'));
    assertDeepEquals(
        [
          FeedbackEvent.OPENED,
          FeedbackEvent.SENDING,
          FeedbackEvent.RESENDING,
          FeedbackEvent.RESENDING,
          FeedbackEvent.FAILED,
        ],
        browserProxy.getArgs('recordEvent'));
    assertFalse(ui.feedbackSent);
  });
});
