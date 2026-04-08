// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/app.js';

import type {AppElement} from 'chrome://feedback/app.js';
import {FeedbackBrowserProxyImpl} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestFeedbackBrowserProxy} from './test_feedback_browser_proxy.js';

const USER_EMAIL = 'dummy_user_email';

suite('FeedbackTest', function() {
  let app: AppElement;
  let browserProxy: TestFeedbackBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  setup(async function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    browserProxy = new TestFeedbackBrowserProxy();
    browserProxy.setUserEmail(USER_EMAIL);
    FeedbackBrowserProxyImpl.setInstance(browserProxy);

    app = await createAppElement();
  });

  async function createAppElement(): Promise<AppElement> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const app: AppElement = document.createElement('feedback-app');
    document.body.appendChild(app);
    await eventToPromise('ready-for-testing', app);
    return app;
  }

  test('Layout', function() {
    // Description
    assertTrue(isVisible(app.getRequiredElement('#free-form-text')));
    assertTrue(isVisible(app.getRequiredElement('#description-text')));

    // Additional Info
    assertTrue(isVisible(app.getRequiredElement('#additional-info-label')));

    // URL
    assertTrue(isVisible(app.getRequiredElement('#page-url-label')));
    assertTrue(isVisible(app.getRequiredElement('#page-url-text')));

    // Email drop-down select
    assertTrue(isVisible(app.getRequiredElement('#user-email')));
    const select =
        app.getRequiredElement<HTMLSelectElement>('#user-email-drop-down');
    assertTrue(isVisible(select));
    assertEquals(2, select.options.length);
    assertFalse(isVisible(app.getRequiredElement('#anonymous-user-option')));

    // Attach File
    assertTrue(isVisible(app.getRequiredElement('#attach-file-label')));
    assertTrue(isVisible(app.getRequiredElement('#attach-file')));
    assertTrue(isVisible(app.getRequiredElement('#attach-file-note')));

    // Consent
    assertTrue(isVisible(app.getRequiredElement('#consent-checkbox')));
    assertTrue(isVisible(app.getRequiredElement('#consent-chk-label')));

    // Screenshot
    const checkbox =
        app.getRequiredElement<HTMLInputElement>('#screenshot-checkbox');
    assertTrue(isVisible(checkbox));
    assertFalse(checkbox.checked);
    assertTrue(isVisible(app.getRequiredElement('#screenshot-chk-label')));

    const image = app.getRequiredElement<HTMLImageElement>('#screenshot-image');
    assertTrue(!!image.src);
    assertTrue(!!image.alt);

    // Privacy note
    assertTrue(isVisible(app.getRequiredElement('#privacy-note')));
  });

  test('ScreenshotFailed', async function() {
    browserProxy.setUserMedia(Promise.reject(
        {name: 'error', message: 'error', constraintName: 'error'}));
    app = await createAppElement();

    const checkbox =
        app.getRequiredElement<HTMLInputElement>('#screenshot-checkbox');
    assertTrue(checkbox.disabled);
    assertFalse(checkbox.checked);

    const image = app.getRequiredElement<HTMLImageElement>('#screenshot-image');
    assertEquals('', image.src);
    assertEquals('', image.alt);
  });

  test('SysInfo', function() {
    const checkbox =
        app.getRequiredElement<HTMLInputElement>('#sys-info-checkbox');
    assertTrue(isVisible(checkbox));
    assertTrue(checkbox.checked);

    assertTrue(isVisible(app.getRequiredElement('#sys-info-label')));

    const sysInfoUrl =
        app.getRequiredElement<HTMLAnchorElement>('#sys-info-url');

    // Prevent clicking system info link from opening a new window.
    sysInfoUrl.href = '#';
    sysInfoUrl.target = '_self';

    sysInfoUrl.click();
    return browserProxy.whenCalled('showSystemInfo');
  });

  test('SendReport', async function() {
    const DESCRIPTION_TEXT = 'feedback test';
    const PAGE_URL = 'chrome://feedback';

    app.getRequiredElement('#description-text').textContent = DESCRIPTION_TEXT;
    app.getRequiredElement<HTMLInputElement>('#page-url-text').value = PAGE_URL;
    app.getRequiredElement<HTMLInputElement>('#screenshot-checkbox').click();

    const button = app.getRequiredElement('#send-report-button');
    button.click();

    const feedbackInfo = await browserProxy.whenCalled('sendFeedback');
    assertEquals(DESCRIPTION_TEXT, feedbackInfo.description);
    assertEquals(PAGE_URL, feedbackInfo.pageUrl);
    assertEquals(USER_EMAIL, feedbackInfo.email);
    assertTrue(!!feedbackInfo.screenshot);
    assertTrue(!!feedbackInfo.systemInformation);
  });

  test('SendReportWithoutUserEmail', async function() {
    app.getRequiredElement('#description-text').textContent = 'test';
    // Select the anonymous-user-option which is always listed after
    // user-email-option when logged in.
    app.getRequiredElement<HTMLSelectElement>('#user-email-drop-down')
        .options[1]!.selected = true;

    const button = app.getRequiredElement('#send-report-button');
    button.click();

    const feedbackInfo = await browserProxy.whenCalled('sendFeedback');
    assertEquals('', feedbackInfo.email);
  });

  test('CloseButtonClosesDialog', function() {
    const button = app.getRequiredElement('#cancel-button');
    button.click();
    return browserProxy.whenCalled('closeDialog');
  });
});

suite('AIFeedbackTest', function() {
  const LOG_ID: string = 'TEST_LOG_ID';
  let app: AppElement;
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

suite('SeaPenFeedbackTest', function() {
  let app: AppElement;
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
      aiMetadata: JSON.stringify({'from_sea_pen': 'true'}),
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

  test('NoScreenshots', function() {
    assertFalse(isVisible(app.getRequiredElement('#screenshot-container')));
  });

  test('OffensiveContainerVisibility', async function() {
    assertTrue(isVisible(app.getRequiredElement('#offensive-container')));
    app.getRequiredElement('#offensive-checkbox').click();
    simulateSendReport();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertTrue(feedbackInfo.isOffensiveOrUnsafe!);
  });

  test('ExcludeServerLogs', async function() {
    assertFalse(isVisible(app.getRequiredElement('#log-id-container')));
    simulateSendReport();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertEquals(undefined, feedbackInfo.aiMetadata);
  });

  test('ExcludeSystemInfo', async function() {
    assertFalse(isVisible(app.getRequiredElement('#sys-info-container')));
    simulateSendReport();
    const feedbackInfo: chrome.feedbackPrivate.FeedbackInfo =
        await browserProxy.whenCalled('sendFeedback');
    assertEquals(false, feedbackInfo.sendHistograms);
  });
});
