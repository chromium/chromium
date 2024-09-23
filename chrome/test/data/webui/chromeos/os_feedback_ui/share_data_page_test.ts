// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-feedback/file_attachment.js';
import 'chrome://os-feedback/share_data_page.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {fakeEmptyFeedbackContext, fakeFeedbackContext, fakeFeedbackContextWithExtraDiagnostics, fakeInternalUserFeedbackContext, fakeLoginFlowFeedbackContext} from 'chrome://os-feedback/fake_data.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FeedbackFlowButtonClickEvent, FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {FileAttachmentElement} from 'chrome://os-feedback/file_attachment.js';
import {setFeedbackServiceProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {FeedbackAppPreSubmitAction} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {ShareDataPageElement} from 'chrome://os-feedback/share_data_page.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

const fakeImageUrl = 'chrome://os_feedback/app_icon_48.png';

suite('shareDataPageTestSuite', () => {
  let page: ShareDataPageElement;

  let feedbackServiceProvider: FakeFeedbackServiceProvider;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  });

  function initializePage() {
    page = document.createElement('share-data-page');
    assert(page);
    page.feedbackContext = fakeEmptyFeedbackContext;
    document.body.appendChild(page);
    return flushTasks();
  }

  function getElement(selector: string): Element|null {
    return page.shadowRoot!.querySelector(selector);
  }

  function getElementContent(selector: string): string {
    const element = page.shadowRoot!.querySelector(selector);
    return element!.textContent!.trim();
  }

  function verifyRecordPreSubmitActionCallCount(
      callCounts: number, action: FeedbackAppPreSubmitAction) {
    assertEquals(
        callCounts,
        feedbackServiceProvider.getRecordPreSubmitActionCallCount(action));
  }

  /**
   * Helper function which will click the send button, wait for the event
   * 'continue-click', and return the detail data of the event.
   */
  async function clickSendAndWait(element: Element):
      Promise<FeedbackFlowButtonClickEvent> {
    const clickPromise = eventToPromise('continue-click', element);

    let eventDetail: FeedbackFlowButtonClickEvent|null = null;
    page.addEventListener(
        'continue-click', (event: FeedbackFlowButtonClickEvent) => {
          eventDetail = event;
        });

    strictQuery('#buttonSend', page.shadowRoot, CrButtonElement).click();

    await clickPromise;

    assertTrue(!!eventDetail);
    assertEquals(
        FeedbackFlowState.SHARE_DATA,
        (eventDetail as FeedbackFlowButtonClickEvent).detail.currentState);

    return eventDetail;
  }

  // Test the page is loaded with expected HTML elements.
  test('shareDataPageLoaded', async () => {
    await initializePage();
    // Verify the title is in the page.
    assertEquals('Send feedback', getElementContent('.page-title'));
    assertTrue(page.i18nExists('pageTitle'));

    // Verify the back button is in the page.
    assertEquals('Back', getElementContent('#buttonBack'));
    assertTrue(page.i18nExists('backButtonLabel'));

    // Verify the send button is in the page.
    assertEquals('Send', getElementContent('#buttonSend'));
    assertTrue(page.i18nExists('sendButtonLabel'));

    // Verify the user email label is in the page.
    assertTrue(page.i18nExists('userEmailLabel'));
    assertEquals('Email', getElementContent('#userEmailLabel'));

    // Verify the aria label of the user email dropdown.
    const userEmailDropDown =
        strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement);
    assertTrue(page.i18nExists('userEmailAriaLabel'));
    assertEquals('Select email', userEmailDropDown.ariaLabel);

    // Verify don't include email address is in the page.
    assertTrue(page.i18nExists('anonymousUser'));
    assertEquals(
        `Don't include email address`, getElementContent('#anonymousUser'));

    // Verify the share diagnostic data label is in the page.
    assertTrue(page.i18nExists('shareDiagnosticDataLabel'));
    assertEquals(
        'Share diagnostic data',
        getElementContent('#shareDiagnosticDataLabel'));

    // Screenshot elements.
    const screenshotCheckbox =
        strictQuery('#screenshotCheckbox', page.shadowRoot, CrCheckboxElement);
    assertTrue(!!screenshotCheckbox);
    assertTrue(page.i18nExists('attachScreenshotCheckboxAriaLabel'));
    assertEquals('Attach screenshot', screenshotCheckbox.ariaDescription);

    assertTrue(page.i18nExists('attachScreenshotLabel'));
    assertEquals('Screenshot', getElementContent('#screenshotCheckLabel'));

    assertTrue(
        !!strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement));
    assertEquals(
        'Preview Screenshot',
        strictQuery('#imageButton', page.shadowRoot, HTMLButtonElement)
            .ariaLabel);
    assertTrue(page.i18nExists('previewImageAriaLabel'));

    // Add file attachment element.
    assertTrue(!!getElement('file-attachment'));

    // Email elements.
    assertEquals('Email', getElementContent('#userEmailLabel'));
    assertTrue(!!strictQuery(
        '#userEmailDropDown', page.shadowRoot, HTMLSelectElement));

    // URL elements.
    assertEquals('Share URL:', getElementContent('#pageUrlLabel'));
    assertTrue(page.i18nExists('sharePageUrlLabel'));
    assertTrue(
        !!strictQuery('#pageUrlCheckbox', page.shadowRoot, CrCheckboxElement));
    assertTrue(
        !!strictQuery('#pageUrlText', page.shadowRoot, HTMLAnchorElement));

    // System info label is a localized string in HTML format.
    assertTrue(getElementContent('#sysInfoCheckboxLabel').length > 0);

    // Performance trace label is a localized string in HTML format.
    assertTrue(getElementContent('#performanceTraceCheckboxLabel').length > 0);

    // Performance trace label is a localized string in HTML format.
    assertTrue(getElementContent('#performanceTraceCheckboxLabel').length > 0);

    // Privacy note is a long localized string in HTML format.
    assertTrue(page.i18nExists('privacyNote'));
    assertEquals(
        'Some account and system information may be sent to Google. We use ' +
            'this information to help address technical issues and improve ' +
            'our services, subject to our Privacy Policy and Terms of ' +
            'Service. To request content changes, go to Legal Help.',
        getElementContent('#privacyNote'));
  });

  // Test the privacy note displayed to logged out users.
  test('privacyNote_loggedOut_users', async () => {
    await initializePage();
    page.feedbackContext = fakeLoginFlowFeedbackContext;
    assertEquals(
        'Some account and system information may be sent to Google. We use ' +
            'this information to help address technical issues and improve ' +
            'our services, subject to our Privacy Policy ' +
            '(https://policies.google.com/privacy) and Terms of Service ' +
            '(https://policies.google.com/terms). To request content changes,' +
            ' go to Legal Help ' +
            '(https://support.google.com/legal/answer/3110420).',
        getElementContent('#privacyNote'));
  });

  // Test the add file section is visible to logged in users.
  test('addFileVisible_loggedIn_users', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    assertNotEquals('Login', page.feedbackContext.categoryTag);

    assertTrue(page.i18nExists('attachFilesLabelLoggedIn'));
    // Add file section is visible.
    assertTrue(isVisible(
        strictQuery('#addFileContainer', page.shadowRoot, HTMLElement)));
    // Attach files Icon should be visible.
    assertTrue(isVisible(
        strictQuery('#attachFilesIcon', page.shadowRoot, HTMLElement)));
    // Attach files label should be "Attach files".
    assertEquals('Attach files', getElementContent('#attachFilesLabel'));
  });

  // Test the add file section is invisible to logged out users.
  test('addFileInvisible_loggedOut_users', async () => {
    await initializePage();
    page.feedbackContext = fakeLoginFlowFeedbackContext;
    assertEquals('Login', page.feedbackContext.categoryTag);

    assertTrue(page.i18nExists('attachFilesLabelLoggedOut'));
    // Add file section is invisible.
    assertFalse(isVisible(
        strictQuery('#addFileContainer', page.shadowRoot, HTMLElement)));
    // Attach files Icon should be invisible.
    assertFalse(isVisible(
        strictQuery('#attachFilesIcon', page.shadowRoot, HTMLElement)));
    // Attach files label should be "Add screenshot".
    assertEquals('Add screenshot', getElementContent('#attachFilesLabel'));
  });

  // Test that the email drop down is populated with two options.
  test('emailDropdownPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const emailDropdown: HTMLSelectElement =
        strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement);
    assertTrue(!!emailDropdown);
    assertEquals(2, emailDropdown.options.length);

    const firstOption = emailDropdown.options.item(0) as HTMLOptionElement;
    assertEquals('test.user2@test.com', firstOption.textContent!.trim());
    assertEquals('test.user2@test.com', firstOption.value.trim());

    const secondOption = emailDropdown.options.item(1);
    assertEquals(
        'Don\'t include email address', secondOption!.textContent!.trim());
    assertEquals('', secondOption!.value!.trim());

    // The user email section should be visible.
    const userEmailElement =
        strictQuery('#userEmail', page.shadowRoot, HTMLElement);
    assertTrue(!!userEmailElement);
    assertTrue(isVisible(userEmailElement));

    // The user user consent checkbox should be visible.
    const consentCheckbox =
        strictQuery('#userConsent', page.shadowRoot, HTMLElement);
    assertTrue(!!consentCheckbox);
    assertTrue(isVisible(consentCheckbox));
  });

  // Test that the email section and consent checkbox is hidden
  // when there is no email.
  test('emailSectionHiddenWithoutEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeEmptyFeedbackContext;

    // The user email section should be hidden.
    const userEmailElement =
        strictQuery('#userEmail', page.shadowRoot, HTMLElement);
    assertTrue(!!userEmailElement);
    assertFalse(isVisible(userEmailElement));

    // The user consent checkbox should be hidden.
    const consentCheckbox =
        strictQuery('#userConsent', page.shadowRoot, HTMLElement);
    assertTrue(!!consentCheckbox);
    assertFalse(isVisible(consentCheckbox));
  });

  test('pageUrlPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    assertEquals('chrome://tab/', getElementContent('#pageUrlText'));
  });

  // Test that the pageUrl section is hidden when the url is empty string.
  test('pageUrlHidden', async () => {
    await initializePage();
    fakeFeedbackContext!.pageUrl!.url = '';
    page.feedbackContext = fakeFeedbackContext;

    // The pageUrl section should be hidden
    const pageUrl = strictQuery('#pageUrl', page.shadowRoot, HTMLElement);
    assertTrue(!!pageUrl);
    assertFalse(isVisible(pageUrl));

    // Change it back otherwise it will effect other tests.
    fakeFeedbackContext!.pageUrl!.url = 'chrome://tab/';
  });

  // Test that the performanceTraceContainer section is hidden when the trace id
  // is zero.
  test('performanceTraceContainerHidden', async () => {
    await initializePage();
    // Trace id will be zero in this context.
    page.feedbackContext = fakeEmptyFeedbackContext;

    // The performanceTraceContainer section should be hidden
    const performanceTraceContainer =
        strictQuery('#performanceTraceContainer', page.shadowRoot, HTMLElement);
    assertTrue(!!performanceTraceContainer);
    assertFalse(isVisible(performanceTraceContainer));

    // Now use the context with a non-zero trace id.
    page.feedbackContext = fakeFeedbackContext;
    // The performanceTraceContainer section should be visible
    assertTrue(isVisible(performanceTraceContainer));
  });

  // Test clicking performanceTraceLink link.
  test('performanceTraceLink', async () => {
    await initializePage();

    // Set up performance trace id.
    page.feedbackContext = fakeFeedbackContext;

    const link =
        strictQuery('#performanceTraceLink', page.shadowRoot, HTMLElement);

    assertEquals('_blank', link.getAttribute('target'));
    // Performance trace id is the last number in the URL, which is 1.
    assertEquals(
        'chrome://slow_trace/tracing.zip#1', link.getAttribute('href'));
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 1: Share pageUrl, do not share system logs.
   */
  test('SendReportSharePageUrlButNotSystemLogs', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    strictQuery('#pageUrlCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;
    strictQuery('#sysInfoCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;

    const report = (await clickSendAndWait(page)).detail!.report;

    assertEquals('chrome://tab/', report!.feedbackContext!.pageUrl!.url);
    assertFalse(report!.includeSystemLogsAndHistograms);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 2: Share system logs, do not share pageUrl.
   */
  test('SendReportShareSystemLogsButNotPageUrl', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    strictQuery('#pageUrlCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;
    strictQuery('#sysInfoCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;

    const request = (await clickSendAndWait(page)).detail.report;
    const pageUrl = request?.feedbackContext?.pageUrl;
    assertFalse(!!pageUrl);
    assertTrue(request!.includeSystemLogsAndHistograms);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 3: Share email and screenshot.
   */
  test('SendReportShareEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    page.screenshotUrl = fakeImageUrl;
    assertEquals(
        fakeImageUrl,
        strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement).src);

    // Select the email.
    strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement)
        .value = 'test.user2@test.com';
    // Select the screenshot.
    strictQuery('#screenshotCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;

    const request = (await clickSendAndWait(page)).detail.report;

    assertEquals('test.user2@test.com', request!.feedbackContext.email);
    assertTrue(request!.includeScreenshot);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 4: Do not share email or screenshot.
   * 4.1) No screenshot and screenshot checkbox is unchecked.
   */
  test('SendReportDoNotShareEmailNoScreenshotUnchecked', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    // When there is not a screenshot.
    page.screenshotUrl = '';
    assertFalse(
        !!strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement)
              .src);

    // Select the "Don't include email address" option.
    strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement)
        .value = '';

    const request = (await clickSendAndWait(page)).detail.report;

    assertFalse(!!request!.feedbackContext.email);
    assertFalse(request!.includeScreenshot);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 4: Do not share email or screenshot.
   * 4.2) No screenshot and screenshot checkbox is checked.
   */
  test('SendReportDoNotShareEmailNoScreenshotChecked', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    // When there is not a screenshot.
    page.screenshotUrl = '';
    assertFalse(
        !!strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement)
              .src);

    // Select the "Don't include email address" option.
    strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement)
        .value = '';

    // When the checkbox is selected but there is not a screenshot.
    strictQuery('#screenshotCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;
    assertFalse(
        !!strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement)
              .src);

    const request = (await clickSendAndWait(page)).detail.report;

    assertFalse(!!request!.feedbackContext.email);
    assertFalse(request!.includeScreenshot);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 4: Do not share email or screenshot.
   * 4.3) Has screenshot but screenshot checkbox is unchecked.
   */
  test('SendReportDoNotShareEmailHasScreenshotUnchecked', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    // Select the "Don't include email address" option.
    strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement)
        .value = '';

    // When there is a screenshot but it is not selected.
    page.screenshotUrl = fakeImageUrl;
    assertEquals(
        fakeImageUrl,
        strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement).src);
    strictQuery('#screenshotCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;

    const request = (await clickSendAndWait(page)).detail.report;

    assertFalse(!!request!.feedbackContext.email);
    assertFalse(request!.includeScreenshot);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 5: Send performance trace id.
   */
  test('SendPerformanceTraceId', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    strictQuery('#performanceTraceCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;

    const report = (await clickSendAndWait(page)).detail.report;

    assertEquals(fakeFeedbackContext.traceId, report!.feedbackContext!.traceId);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 6: Don't send performance trace id.
   */
  test('DontSendPerformanceTraceId', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    strictQuery('#performanceTraceCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;

    const report = (await clickSendAndWait(page)).detail.report;

    assertEquals(0, report!.feedbackContext!.traceId);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 7: Report won't have assistant log flags if isInternalAccount
   * and fromAssistant flag in feedbackContext is false.
   */
  test('ReportWillNotHaveAssistantLogIfFromAssistantSetFalse', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    // The report should not have assistant logs by default.
    strictQuery('#assistantLogsContainer', page.shadowRoot, HTMLElement)
        .hidden = true;
    assertTrue(strictQuery(
                   '#assiatantLogsCheckbox', page.shadowRoot, CrCheckboxElement)
                   .checked);
    const report = (await clickSendAndWait(page)).detail.report;

    assertFalse(report!.feedbackContext!.assistantDebugInfoAllowed);
    assertFalse(report!.feedbackContext!.fromAssistant);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 8: Send assistant log if assistant log checkbox is checked,
   * the report should show assistant Debug Info allowed.
   */
  test('SendAssistantLogWithReport', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;

    assertTrue(
        !!strictQuery('#assistantLogsContainer', page.shadowRoot, HTMLElement));
    strictQuery('#assistantLogsContainer', page.shadowRoot, HTMLElement)
        .hidden = false;
    strictQuery('#assiatantLogsCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;

    const report = (await clickSendAndWait(page)).detail.report;
    assertTrue(report!.feedbackContext!.assistantDebugInfoAllowed);
    assertTrue(report!.feedbackContext!.fromAssistant);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 9: Don't include assistant log if assistant log checkbox is unchecked,
   * the report should show assistant Debug Info not allowed.
   */
  test('SendAssistantLogWithReport', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;

    assertTrue(
        !!strictQuery('#assistantLogsContainer', page.shadowRoot, HTMLElement));
    strictQuery('#assistantLogsContainer', page.shadowRoot, HTMLElement)
        .hidden = false;

    // Uncheck the assistant logs checkbox.
    strictQuery('#assiatantLogsCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;

    const report = (await clickSendAndWait(page)).detail.report;

    assertFalse(report!.feedbackContext!.assistantDebugInfoAllowed);
    assertTrue(report!.feedbackContext!.fromAssistant);
  });

  /**
   * Case 10: Test when user using internal account but feedback is not called
   * from Assistant, and the report should not have fromAssistant and
   * assistantDebugInfoAllowed flags set true.
   */
  test('SendReportWithInternalAccountButNotFromAssistant', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.feedbackContext.fromAssistant = false;

    assertTrue(isVisible(
        strictQuery('#assistantLogsContainer', page.shadowRoot, HTMLElement)));
    assertTrue(strictQuery(
                   '#assiatantLogsCheckbox', page.shadowRoot, CrCheckboxElement)
                   .checked);

    const report = (await clickSendAndWait(page)).detail.report;

    assertFalse(report!.feedbackContext!.assistantDebugInfoAllowed);
    assertFalse(report!.feedbackContext!.fromAssistant);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 11: Share autofill metadata.
   */
  test('SendAutofillMetadataChecked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.feedbackContext.fromAutofill = true;
    page.feedbackContext.autofillMetadata = 'Autofill Metadata';

    assertTrue(isVisible(strictQuery(
        '#autofillCheckboxContainer', page.shadowRoot, HTMLElement)));
    strictQuery('#autofillCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;

    const request = (await clickSendAndWait(page)).detail.report;

    assertTrue(!!request!.feedbackContext.autofillMetadata);
    assertTrue(request!.includeAutofillMetadata);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 12: Do not share autofill metadata.
   */
  test('NotSendAutofillMetadataChecked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.feedbackContext.fromAutofill = true;

    assertTrue(isVisible(strictQuery(
        '#autofillCheckboxContainer', page.shadowRoot, HTMLElement)));
    strictQuery('#autofillCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;

    const request = (await clickSendAndWait(page)).detail.report;

    assertFalse(!!request!.feedbackContext.autofillMetadata);
    assertFalse(request!.includeAutofillMetadata);
  });


  /**
   * Test that the sendWifiDebugLogs flag of the report is set to true when
   * - shouldShowWifiDebugLogsCheckbox = true
   * - wifiDebugLogsCheckbox is checked.
   */
  test('SendWifiDebugLogs_If_Checked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.shouldShowWifiDebugLogsCheckbox = true;
    assertTrue(isVisible(strictQuery(
        '#wifiDebugLogsCheckboxContainer', page.shadowRoot, HTMLElement)));
    strictQuery('#wifiDebugLogsCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;

    const report = (await clickSendAndWait(page)).detail.report;

    assertTrue(report!.sendWifiDebugLogs);
  });


  /**
   * Test that the sendWifiDebugLogs flag of the report is set to false when
   * - shouldShowWifiDebugLogsCheckbox = true
   * - wifiDebugLogsCheckbox is unchecked.
   */
  test('SendWifiDebugLogs_If_Not_Checked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.shouldShowWifiDebugLogsCheckbox = true;
    assertTrue(isVisible(strictQuery(
        '#wifiDebugLogsCheckboxContainer', page.shadowRoot, HTMLElement)));
    strictQuery('#wifiDebugLogsCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;

    const report = (await clickSendAndWait(page)).detail.report;

    assertFalse(report!.sendWifiDebugLogs);
  });

  // Test that the send button will be disabled once clicked.
  test('DisableSendButtonAfterClick', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const sendButton =
        strictQuery('#buttonSend', page.shadowRoot, CrButtonElement);

    assertFalse(sendButton.disabled);

    await clickSendAndWait(page);

    assertTrue(sendButton.disabled);
  });

  // Test that the screenshot checkbox is disabled when no screenshot.
  test('screenshotNotAvailable', async () => {
    await initializePage();
    page.screenshotUrl = '';

    const screenshotCheckbox =
        strictQuery('#screenshotCheckbox', page.shadowRoot, CrCheckboxElement);
    assertTrue(screenshotCheckbox.disabled);

    const screenshotImage =
        strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement);
    assertFalse(!!screenshotImage.src);
  });

  // Test that the screenshot checkbox is enabled when there is a screenshot.
  test('screenshotAvailable', async () => {
    await initializePage();

    const imgUrl = 'chrome://os-feedback/image.png';
    page.screenshotUrl = imgUrl;

    const screenshotCheckbox =
        strictQuery('#screenshotCheckbox', page.shadowRoot, CrCheckboxElement);
    assertFalse(screenshotCheckbox.disabled);

    const screenshotImage =
        strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement);
    assertTrue(!!screenshotImage.src);
    assertEquals(imgUrl, screenshotImage.src);
  });


  // Test that clicking the screenshot will open preview dialog and set the
  // focus on the close dialog icon button.
  test('screenshotPreview', async () => {
    await initializePage();
    verifyRecordPreSubmitActionCallCount(
        0, FeedbackAppPreSubmitAction.kViewedScreenshot);
    page.feedbackContext = fakeFeedbackContext;
    page.screenshotUrl = fakeImageUrl;
    assertEquals(
        fakeImageUrl,
        strictQuery('#screenshotImage', page.shadowRoot, HTMLImageElement).src);

    const closeDialogButton =
        strictQuery('#closeDialogButton', page.shadowRoot, CrButtonElement);
    // The preview dialog's close icon button is not visible.
    assertFalse(isVisible(closeDialogButton));

    // The screenshot is displayed as an image button.
    const imageButton =
        strictQuery('#imageButton', page.shadowRoot, HTMLButtonElement);
    const imageClickPromise = eventToPromise('click', imageButton);
    imageButton.click();
    await imageClickPromise;

    // The preview dialog's close icon button is visible now.
    assertTrue(isVisible(closeDialogButton));
    // The preview dialog's close icon button is focused.
    assertEquals(closeDialogButton, getDeepActiveElement());
    verifyRecordPreSubmitActionCallCount(
        1, FeedbackAppPreSubmitAction.kViewedScreenshot);

    // Press enter should close the preview dialog.
    closeDialogButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    // The preview dialog's close icon button is not visible now.
    assertFalse(isVisible(closeDialogButton));
  });

  /**
   * Test that when when the send button is clicked, the getAttachedFile has
   * been called.
   */
  test('getAttachedFileCalled', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const fileAttachment =
        strictQuery('file-attachment', page.shadowRoot, FileAttachmentElement);
    const fakeFileData = [11, 22, 99];
    fileAttachment.getAttachedFile = async () => {
      const data: BigBuffer = {bytes: fakeFileData} as any;
      return {
        fileName: {path: {path: 'fake.zip'}},
        fileData: data,
      };
    };

    const request = (await clickSendAndWait(page)).detail.report;

    const attachedFile = request!.attachedFile;
    assertTrue(!!attachedFile);
    assertEquals('fake.zip', attachedFile.fileName.path.path);
    assertArrayEquals(fakeFileData, attachedFile!.fileData!.bytes as number[]);
  });

  /**
   * Test that when page initially loaded "user-consent" checkbox is false and
   * expected localize message displayed.
   */
  test('UserConsentGrantedCheckbox_StartsFalse', async () => {
    const expectedUserConsentMessage =
        'Allow Google to email you about this issue';
    await initializePage();

    assertTrue(page.i18nExists('userConsentLabel'));

    const userConsentCheckboxChecked =
        strictQuery('#userConsentCheckbox', page.shadowRoot, CrCheckboxElement)
            .checked;
    const userConsentText = getElementContent('#userConsentLabel');

    assertFalse(userConsentCheckboxChecked);
    assertEquals(expectedUserConsentMessage, userConsentText);
  });

  /**
   * Test that report "contact_user_consent_granted" matches "user-consent"
   * checkbox value.
   */
  test(
      'UserConsentGrantedCheckbox_UpdatesReportContactUserConsentGranted',
      async () => {
        await initializePage();
        page.feedbackContext = fakeFeedbackContext;
        strictQuery('#userConsentCheckbox', page.shadowRoot, CrCheckboxElement)
            .checked = true;
        await flushTasks();

        const reportWithConsent = (await clickSendAndWait(page)).detail.report;

        assertTrue(reportWithConsent!.contactUserConsentGranted);

        page.reEnableSendReportButton();
        page.feedbackContext = fakeFeedbackContext;
        strictQuery('#userConsentCheckbox', page.shadowRoot, CrCheckboxElement)
            .checked = false;
        await flushTasks();

        const reportWithoutConsent =
            (await clickSendAndWait(page)).detail.report;
        assertFalse(reportWithoutConsent!.contactUserConsentGranted);
      });

  /**
   * Test that when report is anonymous (no email provided), "user-consent"
   * checkbox is disabled and value is false.
   */
  test(
      'UserConsentGrantedCheckbox_ReportAnonymous_FalseAndDisabled',
      async () => {
        await initializePage();
        page.feedbackContext = fakeFeedbackContext;
        const disabledInputClass = 'disabled-input-text';

        const consentLabel =
            strictQuery('#userConsentLabel', page.shadowRoot, HTMLElement);
        const consentCheckbox = strictQuery(
            '#userConsentCheckbox', page.shadowRoot, CrCheckboxElement);
        const emailDropdown = strictQuery(
            '#userEmailDropDown', page.shadowRoot, HTMLSelectElement);

        // Select the email.
        emailDropdown.value = 'test.user2@test.com';
        await flushTasks();

        // With email selected and consent not granted.
        assertFalse(consentCheckbox.disabled);
        assertFalse(consentCheckbox.checked);
        assertFalse(consentLabel.classList.contains(disabledInputClass));

        // Check checkbox.
        consentCheckbox.click();
        await flushTasks();

        // With email selected and consent granted.
        assertFalse(consentCheckbox.disabled);
        assertTrue(consentCheckbox.checked);
        assertFalse(consentLabel.classList.contains(disabledInputClass));

        // Select the "Do Not Provide Email" option.
        emailDropdown.value = '';
        emailDropdown.dispatchEvent(new CustomEvent('change'));
        flush();

        // With anonymous email selected and consent not granted.
        assertTrue(consentCheckbox.disabled);
        assertFalse(consentCheckbox.checked);
        assertTrue(consentLabel.classList.contains(disabledInputClass));
      });

  /**
   * Test that when feedback context contains extra_diagnostics matching value
   * is set on report.
   */
  test('AdditionalContext_ExtraDiagnostics', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const reportWithoutExtraDiagnostics =
        (await clickSendAndWait(page)).detail.report;
    assertFalse(
        !!reportWithoutExtraDiagnostics!.feedbackContext!.extraDiagnostics);

    page.reEnableSendReportButton();
    page.feedbackContext = fakeFeedbackContextWithExtraDiagnostics;
    strictQuery('#sysInfoCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = true;
    await flushTasks();

    const reportWithExtraDiagnostics =
        (await clickSendAndWait(page)).detail.report;
    assertEquals(
        fakeFeedbackContextWithExtraDiagnostics.extraDiagnostics,
        reportWithExtraDiagnostics!.feedbackContext!.extraDiagnostics);

    strictQuery('#sysInfoCheckbox', page.shadowRoot, CrCheckboxElement)
        .checked = false;
    page.reEnableSendReportButton();
    const reportNoSysInfo = (await clickSendAndWait(page)).detail.report;
    assertFalse(!!reportNoSysInfo!.feedbackContext!.extraDiagnostics);
  });

  test(
      'WifiDebugLogsCheckboxVisible_When_ShowWifiDebugLogsCheckbox_True',
      async () => {
        await initializePage();
        page.shouldShowWifiDebugLogsCheckbox = true;

        assertTrue(isVisible(strictQuery(
            '#wifiDebugLogsCheckboxContainer', page.shadowRoot, HTMLElement)));
        assertTrue(
            strictQuery(
                '#wifiDebugLogsCheckbox', page.shadowRoot, CrCheckboxElement)
                .checked);
      });

  test(
      'WifiDebugLogsCheckboxInvisible_When_ShowWifiDebugLogsCheckbox_False',
      async () => {
        await initializePage();
        page.shouldShowWifiDebugLogsCheckbox = false;

        assertFalse(isVisible(strictQuery(
            '#wifiDebugLogsCheckboxContainer', page.shadowRoot, HTMLElement)));
      });

  /**
   * Test that when feedback context contains categoryTag matching value
   * is set on report.
   */
  test('AdditionalContext_CategoryTag_Bluetooth', async () => {
    await initializePage();
    page.feedbackContext = fakeEmptyFeedbackContext;

    // Uncheck the "Link Cross Device Dogfood Feedback" checkbox so that only
    // the Bluetooth-specific categoryTag is added to the report.
    const linkCrossDeviceDogfoodFeedbackCheckbox = strictQuery(
        '#linkCrossDeviceDogfoodFeedbackCheckbox', page.shadowRoot,
        CrCheckboxElement);
    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
    assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

    // Uncheck the bluetooth logs checkbox.
    const bluetoothLogsCheckbox = strictQuery(
        '#bluetoothLogsCheckbox', page.shadowRoot, CrCheckboxElement);
    assertTrue(!!bluetoothLogsCheckbox);
    bluetoothLogsCheckbox.checked = false;
    assertFalse(bluetoothLogsCheckbox.checked);

    const reportWithoutCategoryTag =
        (await clickSendAndWait(page)).detail.report;
    assertFalse(!!reportWithoutCategoryTag!.feedbackContext.categoryTag);

    page.reEnableSendReportButton();
    page.feedbackContext = fakeFeedbackContext;
    await flushTasks();

    const reportWithCategoryTag = (await clickSendAndWait(page)).detail.report;
    assertEquals(
        fakeFeedbackContext.categoryTag,
        reportWithCategoryTag!.feedbackContext!.categoryTag);

    // Check the bluetooth logs checkbox. The categoryTag
    // should be BluetoothReportWithLogs, not the tag from url.
    page.reEnableSendReportButton();
    bluetoothLogsCheckbox.checked = true;
    assertTrue(bluetoothLogsCheckbox.checked);
    assertTrue(isVisible(bluetoothLogsCheckbox));
    await flushTasks();

    const reportWithCategoryTagAndBluetoothFlag =
        (await clickSendAndWait(page)).detail.report;
    assertEquals(
        'BluetoothReportWithLogs',
        reportWithCategoryTagAndBluetoothFlag!.feedbackContext!.categoryTag);
  });

  /**
   * Test that when feedback context contains categoryTag matching value
   * is set on report.
   */
  test(
      'AdditionalContext_CategoryTag_LinkCrossDeviceDogfoodFeedback',
      async () => {
        await initializePage();
        page.feedbackContext = fakeEmptyFeedbackContext;

        // Uncheck the bluetooth logs checkbox so that only the "Link Cross
        // Device Dogfood Feedback"-specific categoryTag is added to the report.
        const bluetoothLogsCheckbox = strictQuery(
            '#bluetoothLogsCheckbox', page.shadowRoot, CrCheckboxElement);
        assertTrue(!!bluetoothLogsCheckbox);
        bluetoothLogsCheckbox.checked = false;
        assertFalse(bluetoothLogsCheckbox.checked);

        // Uncheck the "Link Cross Device Dogfood Feedback" checkbox.
        const linkCrossDeviceDogfoodFeedbackCheckbox = strictQuery(
            '#linkCrossDeviceDogfoodFeedbackCheckbox', page.shadowRoot,
            CrCheckboxElement);
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
        assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

        const reportWithoutCategoryTag =
            (await clickSendAndWait(page)).detail.report;
        assertFalse(!!reportWithoutCategoryTag!.feedbackContext.categoryTag);

        page.reEnableSendReportButton();
        page.feedbackContext = fakeFeedbackContext;
        assertTrue(!!page.feedbackContext.categoryTag);
        await flushTasks();

        const reportWithCategoryTag =
            (await clickSendAndWait(page)).detail.report;
        assertEquals(
            fakeFeedbackContext.categoryTag,
            reportWithCategoryTag!.feedbackContext.categoryTag);

        // Check the Link Cross Device Dogfood Feedback checkbox. The
        // categoryTag should be
        // 'linkCrossDeviceDogfoodFeedbackWithoutBluetoothLogs'.
        page.reEnableSendReportButton();
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        linkCrossDeviceDogfoodFeedbackCheckbox.checked = true;
        assertTrue(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

        page.reEnableSendReportButton();

        const reportWithCrossDeviceWithoutBluetoothLogsTag =
            (await clickSendAndWait(page)).detail.report;
        assertEquals(
            'linkCrossDeviceDogfoodFeedbackWithoutBluetoothLogs',
            reportWithCrossDeviceWithoutBluetoothLogsTag!.feedbackContext
                .categoryTag);
      });

  /**
   * Test that when feedback context contains categoryTag matching value
   * is set on report.
   */
  test(
      'AdditionalContext_CategoryTag_BluetoothLogsAndLinkCrossDeviceDogfoodFeedback',
      async () => {
        await initializePage();
        page.feedbackContext = fakeFeedbackContext;

        // Check both the "Link Cross Device Dogfood Feedback" and Bluetooth
        // logs checkboxes. The categoryTag should then be
        // 'linkCrossDeviceDogfoodFeedbackWithBluetoothLogs'.
        const linkCrossDeviceDogfoodFeedbackCheckbox = strictQuery(
            '#linkCrossDeviceDogfoodFeedbackCheckbox', page.shadowRoot,
            CrCheckboxElement);
        linkCrossDeviceDogfoodFeedbackCheckbox.checked = true;
        assertTrue(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

        const bluetoothLogsCheckbox = strictQuery(
            '#bluetoothLogsCheckbox', page.shadowRoot, CrCheckboxElement);
        assertTrue(!!bluetoothLogsCheckbox);
        bluetoothLogsCheckbox.checked = true;
        assertTrue(bluetoothLogsCheckbox.checked);

        await flushTasks();

        const reportWithCrossDeviceWithBluetoothLogsTag =
            (await clickSendAndWait(page)).detail.report;
        assertEquals(
            'linkCrossDeviceDogfoodFeedbackWithBluetoothLogs',
            reportWithCrossDeviceWithBluetoothLogsTag!.feedbackContext
                .categoryTag);
      });

  /**
   * Test that openMetricsDialog and recordPreSubmitAction are called when
   * #histogramsLink ("metrics") link is clicked.
   */
  test('openMetricsDialog', async () => {
    await initializePage();

    assertEquals(0, feedbackServiceProvider.getOpenMetricsDialogCallCount());
    verifyRecordPreSubmitActionCallCount(
        0, FeedbackAppPreSubmitAction.kViewedMetrics);

    strictQuery('#histogramsLink', page.shadowRoot, HTMLAnchorElement).click();

    assertEquals(1, feedbackServiceProvider.getOpenMetricsDialogCallCount());
    verifyRecordPreSubmitActionCallCount(
        1, FeedbackAppPreSubmitAction.kViewedMetrics);
  });

  /**
   * Test that openSystemInfoDialog and recordPreSubmitAction are called when
   * #sysInfoLink ("system and app info") link is clicked.
   */
  test('openSystemInfoDialog', async () => {
    await initializePage();

    assertEquals(0, feedbackServiceProvider.getOpenSystemInfoDialogCallCount());
    verifyRecordPreSubmitActionCallCount(
        0, FeedbackAppPreSubmitAction.kViewedSystemAndAppInfo);

    strictQuery('#sysInfoLink', page.shadowRoot, HTMLAnchorElement).click();

    assertEquals(1, feedbackServiceProvider.getOpenSystemInfoDialogCallCount());
    verifyRecordPreSubmitActionCallCount(
        1, FeedbackAppPreSubmitAction.kViewedSystemAndAppInfo);
  });

  /**
   * Test that openAutofillDialog and recordPreSubmitAction are called when
   * #autofillMetadataUrl ("autofill metadata") link is clicked.
   */
  test('openAutofillDialog', async () => {
    await initializePage();

    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.feedbackContext.fromAutofill = true;
    page.feedbackContext.autofillMetadata = '{}';

    assertEquals(0, feedbackServiceProvider.getOpenAutofillDialogCallCount());
    verifyRecordPreSubmitActionCallCount(
        0, FeedbackAppPreSubmitAction.kViewedAutofillMetadata);

    strictQuery('#autofillMetadataUrl', page.shadowRoot, HTMLAnchorElement)
        .click();

    assertEquals(1, feedbackServiceProvider.getOpenAutofillDialogCallCount());
    verifyRecordPreSubmitActionCallCount(
        1, FeedbackAppPreSubmitAction.kViewedAutofillMetadata);
  });

  /**
   * Test that clicking the #bluetoothLogsLink will open the dialog and set the
   * focus on the close dialog icon button.
   */
  test('openBluetoothLogsDialog', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    // The bluetooth dialog is not visible as default.
    const closeDialogButton = strictQuery(
        '#bluetoothDialogDoneButton', page.shadowRoot, CrButtonElement);
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #bluetoothLogsLink, the dialog pops up.
    strictQuery('#bluetoothLogsInfoLink', page.shadowRoot, HTMLAnchorElement)
        .click();
    assertTrue(isVisible(closeDialogButton));

    // The preview dialog's close icon button is focused.
    assertEquals(closeDialogButton, getDeepActiveElement());

    // Press enter should close the preview dialog.
    closeDialogButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    // The preview dialog's close icon button is not visible now.
    assertFalse(isVisible(closeDialogButton));
  });

  /**
   * Test that WifiDebugLogs dialog contains a title.
   */
  test('titleForWifiDebugLogsDialog', async () => {
    await initializePage();
    assertEquals(
        'Sharing Wi-Fi Debug Logs',
        getElementContent('#wifiDebugLogsDialogTitle'));
  });

  /**
   * Test that clicking the "View more details" link will open the dialog and
   * set the focus on the close dialog icon button.
   */
  test('openWifiDebugLogsDialog', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;

    // The dialog is not visible as default.
    const closeDialogButton = strictQuery(
        '#wifiDebugLogsDialogDoneButton', page.shadowRoot, CrButtonElement);
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #wifiDebugLogsInfoLink, the dialog pops up.
    const dialog =
        strictQuery('#wifiDebugLogsDialog', page.shadowRoot, HTMLElement);
    const dialogOpenedEvent = eventToPromise('cr-dialog-open', dialog);
    strictQuery('#wifiDebugLogsInfoLink', page.shadowRoot, HTMLElement).click();
    await dialogOpenedEvent;

    assertTrue(isVisible(closeDialogButton));

    // The preview dialog's close icon button is focused.
    assertEquals(closeDialogButton, getDeepActiveElement());

    // Press enter should close the preview dialog.
    closeDialogButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    // The preview dialog's close icon button is not visible now.
    assertFalse(isVisible(closeDialogButton));
  });

  /**
   * Test that clicking the #linkCrossDeviceDogfoodFeedbackInfoLink will open
   * the dialog and set the focus on the close dialog icon button.
   */
  test('openLinkCrossDeviceDogfoodFeedbackDialog', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    // The "Link Cross Device Dogfood Feedback" dialog is not visible as
    // default.
    const closeDialogButton = strictQuery(
        '#linkCrossDeviceDogfoodFeedbackDialogDoneButton', page.shadowRoot,
        CrButtonElement);
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #linkCrossDeviceDogfoodFeedbackLink, the dialog pops
    // up.
    strictQuery(
        '#linkCrossDeviceDogfoodFeedbackInfoLink', page.shadowRoot,
        HTMLAnchorElement)
        .click();
    assertTrue(isVisible(closeDialogButton));

    // The preview dialog's close icon button is focused.
    assertEquals(closeDialogButton, getDeepActiveElement());

    // Press enter should close the preview dialog.
    closeDialogButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    // The preview dialog's close icon button is not visible now.
    assertFalse(isVisible(closeDialogButton));
  });

  /**
   * Test that clicking the #assistantLogsLink will open the dialog and set the
   * focus on the close dialog icon button.
   */
  test('openAssistantLogsDialog', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    // The assistant dialog is not visible as default.
    const closeDialogButton = strictQuery(
        '#assistantDialogDoneButton', page.shadowRoot, CrButtonElement);
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #bluetoothLogsLink, the dialog pops up.
    strictQuery('#assistantLogsLink', page.shadowRoot, HTMLAnchorElement)
        .click();
    assertTrue(isVisible(closeDialogButton));

    // The preview dialog's close icon button is focused.
    assertEquals(closeDialogButton, getDeepActiveElement());

    // Press enter should close the preview dialog.
    closeDialogButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    // The preview dialog's close icon button is not visible now.
    assertFalse(isVisible(closeDialogButton));
  });

  /**
   * Test that sendBluetoothLogs flag is true and categoryTag is marked as
   * 'BluetoothReportWithLogs' when bluetooth logs checkbox is checked.
   */
  test('sendReportWithBluetoothLogsFlagChecked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    await flushTasks();

    // Fake internal google account is used for login. The bluetooth logs
    // checkbox container should be visible.
    assertEquals(
        strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement)
            .value,
        'test.user@google.com');
    strictQuery('#bluetoothCheckboxContainer', page.shadowRoot, HTMLElement)
        .hidden = false;

    const bluetoothLogsCheckbox = strictQuery(
        '#bluetoothLogsCheckbox', page.shadowRoot, CrCheckboxElement);
    const linkCrossDeviceDogfoodFeedbackCheckbox = strictQuery(
        '#linkCrossDeviceDogfoodFeedbackCheckbox', page.shadowRoot,
        CrCheckboxElement);

    // Check the bluetoothLogs checkbox, it is default to be checked.
    assertTrue(!!bluetoothLogsCheckbox);
    assertTrue(bluetoothLogsCheckbox.checked);

    // Uncheck the "Link Cross Device Dogfood Feedback" checkbox so that only
    // the Bluetooth-specific categoryTag is added to the report.
    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
    assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

    // Report should have sendBluetoothLogs flag true, and category marked as
    // "BluetoothReportWithLogs".
    const requestWithBluetoothFlag =
        (await clickSendAndWait(page)).detail.report;

    assertTrue(requestWithBluetoothFlag!.sendBluetoothLogs);
    assertTrue(!!requestWithBluetoothFlag!.feedbackContext.categoryTag);
    assertEquals(
        'BluetoothReportWithLogs',
        requestWithBluetoothFlag!.feedbackContext.categoryTag);
  });

  /**
   * Test that sendBluetoothLogs flag is false and categoryTag is not marked as
   * 'BluetoothReportWithLogs' when bluetooth logs checkbox is unchecked.
   */
  test('sendReportWithoutBluetoothLogsFlagChecked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    await flushTasks();

    // Fake internal google account is used for login. The bluetooth logs
    // checkbox container should be visible.
    assertEquals(
        strictQuery('#userEmailDropDown', page.shadowRoot, HTMLSelectElement)
            .value,
        'test.user@google.com');
    strictQuery('#bluetoothCheckboxContainer', page.shadowRoot, HTMLElement)
        .hidden = false;

    // BluetoothLogs checkbox is default to be checked.
    const bluetoothLogsCheckbox = strictQuery(
        '#bluetoothLogsCheckbox', page.shadowRoot, CrCheckboxElement);
    assertTrue(!!bluetoothLogsCheckbox);
    assertTrue(bluetoothLogsCheckbox.checked);

    // Uncheck the "Link Cross Device Dogfood Feedback" checkbox so that only
    // the Bluetooth-specific categoryTag is added to the report.
    const linkCrossDeviceDogfoodFeedbackCheckbox = strictQuery(
        '#linkCrossDeviceDogfoodFeedbackCheckbox', page.shadowRoot,
        CrCheckboxElement);
    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
    assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

    // Verify that unchecking the checkbox will remove the flag in the report.
    bluetoothLogsCheckbox.click();
    assertFalse(bluetoothLogsCheckbox.checked);
    await flushTasks();

    // Report should not have sendBluetoothLogs flag,
    // and category should not be marked as "BluetoothReportWithLogs".
    const requestWithoutBluetoothFlag =
        (await clickSendAndWait(page)).detail.report;

    assertFalse(requestWithoutBluetoothFlag!.sendBluetoothLogs);
    assertFalse(!!requestWithoutBluetoothFlag!.feedbackContext.categoryTag);
  });
});
