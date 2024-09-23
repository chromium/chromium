// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CastFeedbackUiElement, FeedbackUiBrowserProxy} from 'chrome://cast-feedback/cast_feedback_ui.js';
import {FeedbackEvent, FeedbackUiBrowserProxyImpl} from 'chrome://cast-feedback/cast_feedback_ui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestFeedbackUiBrowserProxy extends TestBrowserProxy implements
    FeedbackUiBrowserProxy {
  timesToFail: number = 0;
  resolver: PromiseResolver<void> = new PromiseResolver();

  constructor() {
    super(['recordEvent', 'sendFeedback']);
  }

  recordEvent(event: FeedbackEvent) {
    this.methodCalled('recordEvent', event);
    if (event === FeedbackEvent.SUCCEEDED || event === FeedbackEvent.FAILED) {
      this.resolver.resolve();
    }
  }

  sendFeedback(info: chrome.feedbackPrivate.FeedbackInfo) {
    this.methodCalled('sendFeedback', info);
    return Promise.resolve({
      status: this.getCallCount('sendFeedback') > this.timesToFail ?
          chrome.feedbackPrivate.Status.SUCCESS :
          chrome.feedbackPrivate.Status.DELAYED,
      landingPageType: chrome.feedbackPrivate.LandingPageType.NORMAL,
    });
  }
}

suite('Suite', function() {
  let browserProxy: TestFeedbackUiBrowserProxy;
  let ui: CastFeedbackUiElement;
  const TEST_COMMENT: string = 'test comment';

  async function submit() {
    const textArea = ui.shadowRoot!.querySelector('textarea');
    assertTrue(!!textArea);
    textArea.value = TEST_COMMENT;
    textArea.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    const submitButton =
        ui.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!submitButton);
    assertFalse(submitButton.disabled);
    submitButton.click();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestFeedbackUiBrowserProxy();
    FeedbackUiBrowserProxyImpl.setInstance(browserProxy);
    ui = document.createElement('feedback-ui');
    document.body.appendChild(ui);
  });

  test('Success', async function() {
    browserProxy.timesToFail = 1;
    ui.resendDelayMs = 50;
    ui.maxResendAttempts = 2;
    await submit();
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
    await submit();
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
