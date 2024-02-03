// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/app.js';

import {FeedbackAppElement} from 'chrome://feedback/app.js';
import {FeedbackBrowserProxyImpl} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestFeedbackBrowserProxy} from './test_feedback_browser_proxy.js';

suite('FeedbackTest', function() {
  let app: FeedbackAppElement;
  let browserProxy: TestFeedbackBrowserProxy;

  setup(async function() {
    // Signal to the prod page that test setup steps have completed.
    browserProxy = new TestFeedbackBrowserProxy();
    FeedbackBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('feedback-app');
    document.body.appendChild(app);
    await eventToPromise('ready-for-testing', app);
  });

  test('CloseButtonClosesDialog', function() {
    const button = app.getRequiredElement('#cancel-button');
    button.click();
    return browserProxy.whenCalled('closeDialog');
  });
});

suite('AIFeedbackTest', function() {
  const LOG_ID: string = 'TEST_LOG_ID';
  let app: FeedbackAppElement;
  let browserProxy: TestFeedbackBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  setup(async function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    // Signal to the prod page that test setup steps have completed.
    browserProxy = new TestFeedbackBrowserProxy();
    browserProxy.setDialogArguments(JSON.stringify({
      flow: chrome.feedbackPrivate.FeedbackFlow.AI,
      categoryTag: 'test',
      aiMetadata: JSON.stringify({log_id: LOG_ID}),
    }));
    FeedbackBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('feedback-app');
    document.body.appendChild(app);
    await eventToPromise('ready-for-testing', app);
  });

  function simulateSendReport() {
    // Make sure description is not empty and send button is not disabled.
    app.getRequiredElement<HTMLTextAreaElement>('#description-text').value =
        'test';
    const button =
        app.getRequiredElement<HTMLButtonElement>('#send-report-button');
    // Send button is being disabled after click in production code, but in
    // tests we want to be able to click on the button multiple times.
    button.disabled = false;
    button.click();
  }

  test('Description', function() {
    assertEquals(
        loadTimeData.getString('freeFormTextAi'),
        app.getRequiredElement('#free-form-text').textContent);
  });

  test('NoEmail', function() {
    assertFalse(isVisible(app.getRequiredElement('#user-email')));
    assertFalse(isVisible(app.getRequiredElement('#consent-container')));
  });

  test('OffensiveContainerVisibility', async function() {
    assertTrue(isVisible(app.getRequiredElement('#offensive-container')));
    app.getRequiredElement('#offensive-checkbox').click();
    simulateSendReport();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertTrue(feedbackInfo.isOffensiveOrUnsafe!);
  });

  test('IncludeServerLogs', async function() {
    assertTrue(isVisible(app.getRequiredElement('#log-id-container')));
    simulateSendReport();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertDeepEquals({log_id: LOG_ID}, JSON.parse(feedbackInfo.aiMetadata!));
  });

  test('ExcludeServerLogs', async function() {
    assertTrue(isVisible(app.getRequiredElement('#log-id-container')));
    app.getRequiredElement('#log-id-checkbox').click();
    simulateSendReport();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertEquals(undefined, feedbackInfo.aiMetadata);
  });
});
