// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FeedbackBrowserProxyImpl} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestFeedbackBrowserProxy} from './test_feedback_browser_proxy.js';

declare global {
  interface Window {
    whenTestSetupDoneResolver: {resolve: () => Promise<void>};
  }
}

suite('FeedbackTest', function() {
  let browserProxy: TestFeedbackBrowserProxy;

  suiteSetup(function() {
    const whenReadyForTesting =
        eventToPromise('ready-for-testing', document.documentElement);

    // Signal to the prod page that test setup steps have completed.
    browserProxy = new TestFeedbackBrowserProxy();
    FeedbackBrowserProxyImpl.setInstance(browserProxy);
    window.whenTestSetupDoneResolver.resolve();

    // Wait for any remaining prod code executes before test cases execute.
    return whenReadyForTesting;
  });

  teardown(function() {
    browserProxy.reset();

    // Note: The UI is not recreated between tests, so must clear any state that
    // could leak between tests here.
  });

  test('CloseButtonClosesDialog', function() {
    const button = getRequiredElement('cancel-button');
    button.click();
    return browserProxy.whenCalled('closeDialog');
  });
});

suite('AIFeedbackTest', function() {
  let browserProxy: TestFeedbackBrowserProxy;

  suiteSetup(function() {
    const whenReadyForTesting =
        eventToPromise('ready-for-testing', document.documentElement);

    // Signal to the prod page that test setup steps have completed.
    browserProxy = new TestFeedbackBrowserProxy();
    browserProxy.setDialogArguments(JSON.stringify(
        {flow: chrome.feedbackPrivate.FeedbackFlow.AI, categoryTag: 'test'}));
    FeedbackBrowserProxyImpl.setInstance(browserProxy);
    window.whenTestSetupDoneResolver.resolve();

    // Wait for any remaining prod code executes before test cases execute.
    return whenReadyForTesting;
  });

  teardown(function() {
    browserProxy.reset();

    // Note: The UI is not recreated between tests, so must clear any state that
    // could leak between tests here.
  });

  test('Description', function() {
    assertEquals(
        loadTimeData.getString('freeFormTextAi'),
        getRequiredElement('free-form-text').textContent);
  });

  test('NoEmail', function() {
    assertFalse(isVisible(getRequiredElement('user-email')));
    assertFalse(isVisible(getRequiredElement('consent-container')));
  });

  test('OffensiveContainerVisibility', async function() {
    assertTrue(isVisible(getRequiredElement('offensive-container')));
    getRequiredElement('offensive-checkbox').click();
    getRequiredElement<HTMLTextAreaElement>('description-text').value = 'test';
    getRequiredElement('send-report-button').click();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertTrue(feedbackInfo.isOffensiveOrUnsafe!);
  });
});
