// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';

import {fakeEmptyFeedbackContext, fakeFeedbackContext, fakeInternalUserFeedbackContext, fakeLoginFlowFeedbackContext} from 'chrome://os-feedback/fake_data.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {FeedbackAppPreSubmitAction, FeedbackContext} from 'chrome://os-feedback/feedback_types.js';
import {setFeedbackServiceProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {ShareDataPageElement} from 'chrome://os-feedback/share_data_page.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {mojoString16ToString, stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise, isVisible} from '../test_util.js';

/** @type {string} */
const fakeImageUrl = 'chrome://os_feedback/app_icon_48.png';

/**
 * @suppress {missingProperties} for test.skip is not defined in mocha-2.5.js
 */
export function shareDataPageTestSuite() {
  /** @type {?ShareDataPageElement} */
  let page = null;

  /** @type {?FakeFeedbackServiceProvider} */
  let feedbackServiceProvider;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;

    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!ShareDataPageElement} */ (
            document.createElement('share-data-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /**
   * @param {string} selector
   * @returns {Element|null}
   */
  function getElement(selector) {
    const element = page.shadowRoot.querySelector(selector);
    return element;
  }

  /**
   * @param {string} selector
   * @returns {string}
   */
  function getElementContent(selector) {
    const element = getElement(selector);
    assertTrue(!!element);
    return element.textContent.trim();
  }

  /**
   * @param {number} callCounts
   * @param {FeedbackAppPreSubmitAction} action
   * @private
   */
  function verifyRecordPreSubmitActionCallCount(callCounts, action) {
    assertEquals(
        callCounts,
        feedbackServiceProvider.getRecordPreSubmitActionCallCount(action));
  }

  /**
   * Helper function which will click the send button, wait for the event
   * 'continue-click', and return the detail data of the event.
   * @param {!Element} element
   */
  async function clickSendAndWait(element) {
    const clickPromise = eventToPromise('continue-click', element);

    let eventDetail;
    page.addEventListener('continue-click', (event) => {
      eventDetail = event.detail;
    });

    getElement('#buttonSend').click();

    await clickPromise;

    assertTrue(!!eventDetail);
    assertEquals(FeedbackFlowState.SHARE_DATA, eventDetail.currentState);

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
    const userEmailDropDown = getElement('#userEmailDropDown');
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
    const screenshotCheckbox = getElement('#screenshotCheckbox');
    assertTrue(!!screenshotCheckbox);
    assertTrue(page.i18nExists('attachScreenshotCheckboxAriaLabel'));
    assertEquals('Attach screenshot', screenshotCheckbox.ariaDescription);

    assertTrue(page.i18nExists('attachScreenshotLabel'));
    assertEquals('Screenshot', getElementContent('#screenshotCheckLabel'));

    assertTrue(!!getElement('#screenshotImage'));
    assertEquals('Preview Screenshot', getElement('#imageButton').ariaLabel);
    assertTrue(page.i18nExists('previewImageAriaLabel'));

    // Add file attachment element.
    assertTrue(!!getElement('file-attachment'));

    // Email elements.
    assertEquals('Email', getElementContent('#userEmailLabel'));
    assertTrue(!!getElement('#userEmailDropDown'));

    // URL elements.
    assertEquals('Share URL:', getElementContent('#pageUrlLabel'));
    assertTrue(page.i18nExists('sharePageUrlLabel'));
    assertTrue(!!getElement('#pageUrlCheckbox'));
    assertTrue(!!getElement('#pageUrlText'));

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
    assertTrue(isVisible(getElement('#addFileContainer')));
    // Attach files Icon should be visible.
    assertTrue(isVisible(getElement('#attachFilesIcon')));
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
    assertFalse(isVisible(getElement('#addFileContainer')));
    // Attach files Icon should be invisible.
    assertFalse(isVisible(getElement('#attachFilesIcon')));
    // Attach files label should be "Add screenshot".
    assertEquals('Add screenshot', getElementContent('#attachFilesLabel'));
  });

  // Test that the email drop down is populated with two options.
  test('emailDropdownPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const emailDropdown = getElement('#userEmailDropDown');
    assertTrue(!!emailDropdown);
    assertEquals(2, emailDropdown.options.length);

    const firstOption = emailDropdown.options.item(0);
    assertEquals('test.user2@test.com', firstOption.textContent.trim());
    assertEquals('test.user2@test.com', firstOption.value.trim());

    const secondOption = emailDropdown.options.item(1);
    assertEquals(
        'Don\'t include email address', secondOption.textContent.trim());
    assertEquals('', secondOption.value.trim());

    // The user email section should be visible.
    const userEmailElement = getElement('#userEmail');
    assertTrue(!!userEmailElement);
    assertTrue(isVisible(userEmailElement));

    // The user user consent checkbox should be visible.
    const consentCheckbox = getElement('#userConsent');
    assertTrue(!!consentCheckbox);
    assertTrue(isVisible(consentCheckbox));
  });

  // Test that the email section and consent checkbox is hidden
  // when there is no email.
  test('emailSectionHiddenWithoutEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeEmptyFeedbackContext;

    // The user email section should be hidden.
    const userEmailElement = getElement('#userEmail');
    assertTrue(!!userEmailElement);
    assertFalse(isVisible(userEmailElement));

    // The user consent checkbox should be hidden.
    const consentCheckbox = getElement('#userConsent');
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
    fakeFeedbackContext.pageUrl.url = '';
    page.feedbackContext = fakeFeedbackContext;

    // The pageUrl section should be hidden
    const pageUrl = getElement('#pageUrl');
    assertTrue(!!pageUrl);
    assertFalse(isVisible(pageUrl));

    // Change it back otherwise it will effect other tests.
    fakeFeedbackContext.pageUrl.url = 'chrome://tab/';
  });

  // Test that the performanceTraceContainer section is hidden when the trace id
  // is zero.
  test('performanceTraceContainerHidden', async () => {
    await initializePage();
    // Trace id will be zero in this context.
    page.feedbackContext = fakeEmptyFeedbackContext;

    // The performanceTraceContainer section should be hidden
    const performanceTraceContainer = getElement('#performanceTraceContainer');
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

    const link = getElement('#performanceTraceLink');

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

    getElement('#pageUrlCheckbox').checked = true;
    getElement('#sysInfoCheckbox').checked = false;

    const eventDetail = await clickSendAndWait(page);

    assertEquals(
        'chrome://tab/', eventDetail.report.feedbackContext.pageUrl.url);
    assertFalse(eventDetail.report.includeSystemLogsAndHistograms);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 2: Share system logs, do not share pageUrl.
   */
  test('SendReportShareSystemLogsButNotPageUrl', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    getElement('#pageUrlCheckbox').checked = false;
    getElement('#sysInfoCheckbox').checked = true;

    const request = (await clickSendAndWait(page)).report;

    assertFalse(!!request.feedbackContext.pageUrl);
    assertTrue(request.includeSystemLogsAndHistograms);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 3: Share email and screenshot.
   */
  test('SendReportShareEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    page.screenshotUrl = fakeImageUrl;
    assertEquals(fakeImageUrl, getElement('#screenshotImage').src);

    // Select the email.
    getElement('#userEmailDropDown').value = 'test.user2@test.com';
    // Select the screenshot.
    getElement('#screenshotCheckbox').checked = true;

    const request = (await clickSendAndWait(page)).report;

    assertEquals('test.user2@test.com', request.feedbackContext.email);
    assertTrue(request.includeScreenshot);
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
    assertFalse(!!getElement('#screenshotImage').src);

    // Select the "Don't include email address" option.
    getElement('#userEmailDropDown').value = '';

    const request = (await clickSendAndWait(page)).report;

    assertFalse(!!request.feedbackContext.email);
    assertFalse(request.includeScreenshot);
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
    assertFalse(!!getElement('#screenshotImage').src);

    // Select the "Don't include email address" option.
    getElement('#userEmailDropDown').value = '';

    // When the checkbox is selected but there is not a screenshot.
    getElement('#screenshotCheckbox').checked = true;
    assertFalse(!!getElement('#screenshotImage').src);

    const request = (await clickSendAndWait(page)).report;

    assertFalse(!!request.feedbackContext.email);
    assertFalse(request.includeScreenshot);
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
    getElement('#userEmailDropDown').value = '';

    // When there is a screenshot but it is not selected.
    page.screenshotUrl = fakeImageUrl;
    assertEquals(fakeImageUrl, getElement('#screenshotImage').src);
    getElement('#screenshotCheckbox').checked = false;

    const request = (await clickSendAndWait(page)).report;

    assertFalse(!!request.feedbackContext.email);
    assertFalse(request.includeScreenshot);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 5: Send performance trace id.
   */
  test('SendPerformanceTraceId', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    getElement('#performanceTraceCheckbox').checked = true;

    const report = (await clickSendAndWait(page)).report;

    assertEquals(fakeFeedbackContext.traceId, report.feedbackContext.traceId);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 6: Don't send performance trace id.
   */
  test('DontSendPerformanceTraceId', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    getElement('#performanceTraceCheckbox').checked = false;

    const report = (await clickSendAndWait(page)).report;

    assertEquals(0, report.feedbackContext.traceId);
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
    getElement('#assistantLogsContainer').hidden = true;
    assertTrue(getElement('#assiatantLogsCheckbox').checked);
    const report = (await clickSendAndWait(page)).report;

    assertFalse(report.feedbackContext.assistantDebugInfoAllowed);
    assertFalse(report.feedbackContext.fromAssistant);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 8: Send assistant log if assistant log checkbox is checked,
   * the report should show assistant Debug Info allowed.
   */
  test('SendAssistantLogWithReport', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;

    assertTrue(!!getElement('#assistantLogsContainer'));
    getElement('#assistantLogsContainer').hidden = false;
    getElement('#assiatantLogsCheckbox').checked = true;

    const report = (await clickSendAndWait(page)).report;
    assertTrue(report.feedbackContext.assistantDebugInfoAllowed);
    assertTrue(report.feedbackContext.fromAssistant);
  });

  /**
   * Test that when the send button is clicked, an on-continue is fired.
   * Case 9: Don't include assistant log if assistant log checkbox is unchecked,
   * the report should show assistant Debug Info not allowed.
   */
  test('SendAssistantLogWithReport', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;

    assertTrue(!!getElement('#assistantLogsContainer'));
    getElement('#assistantLogsContainer').hidden = false;

    // Uncheck the assistant logs checkbox.
    getElement('#assiatantLogsCheckbox').checked = false;

    const report = (await clickSendAndWait(page)).report;

    assertFalse(report.feedbackContext.assistantDebugInfoAllowed);
    assertTrue(report.feedbackContext.fromAssistant);
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

    assertTrue(isVisible(getElement('#assistantLogsContainer')));
    assertTrue(getElement('#assiatantLogsCheckbox').checked);

    const report = (await clickSendAndWait(page)).report;

    assertFalse(report.feedbackContext.assistantDebugInfoAllowed);
    assertFalse(report.feedbackContext.fromAssistant);
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

    assertTrue(isVisible(getElement('#autofillCheckboxContainer')));
    getElement('#autofillCheckbox').checked = true;

    const request = (await clickSendAndWait(page)).report;

    assertTrue(!!request.feedbackContext.autofillMetadata);
    assertTrue(request.includeAutofillMetadata);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 12: Do not share autofill metadata.
   */
  test('NotSendAutofillMetadataChecked', async () => {
    await initializePage();
    page.feedbackContext = fakeInternalUserFeedbackContext;
    page.feedbackContext.fromAutofill = true;

    assertTrue(isVisible(getElement('#autofillCheckboxContainer')));
    getElement('#autofillCheckbox').checked = false;

    const request = (await clickSendAndWait(page)).report;

    assertFalse(!!request.feedbackContext.autofillMetadata);
    assertFalse(request.includeAutofillMetadata);
  });

  // Test that the send button will be disabled once clicked.
  test('DisableSendButtonAfterClick', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const sendButton = getElement('#buttonSend');

    assertFalse(sendButton.disabled);

    await clickSendAndWait(page);

    assertTrue(sendButton.disabled);
  });

  // Test that the screenshot checkbox is disabled when no screenshot.
  test('screenshotNotAvailable', async () => {
    await initializePage();
    page.screenshotUrl = '';

    const screenshotCheckbox = getElement('#screenshotCheckbox');
    assertTrue(screenshotCheckbox.disabled);

    const screenshotImage = getElement('#screenshotImage');
    assertFalse(!!screenshotImage.src);
  });

  // Test that the screenshot checkbox is enabled when there is a screenshot.
  test('screenshotAvailable', async () => {
    await initializePage();

    const imgUrl = 'chrome://os-feedback/image.png';
    page.screenshotUrl = imgUrl;

    const screenshotCheckbox = getElement('#screenshotCheckbox');
    assertFalse(screenshotCheckbox.disabled);

    const screenshotImage = getElement('#screenshotImage');
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
    assertEquals(fakeImageUrl, getElement('#screenshotImage').src);

    const closeDialogButton = getElement('#closeDialogButton');
    // The preview dialog's close icon button is not visible.
    assertFalse(isVisible(closeDialogButton));

    // The screenshot is displayed as an image button.
    const imageButton = /** @type {!Element} */ (getElement('#imageButton'));
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

    const fileAttachment = getElement('file-attachment');
    const fakeFileData = [11, 22, 99];
    fileAttachment.getAttachedFile = async () => {
      return {
        fileName: stringToMojoString16('fake.zip'),
        fileData: {
          bytes: fakeFileData,
        },
      };
    };

    const request = (await clickSendAndWait(page)).report;

    const attachedFile = request.attachedFile;
    assertTrue(!!attachedFile);
    assertEquals('fake.zip', mojoString16ToString(attachedFile.fileName));
    assertArrayEquals(
        fakeFileData,
        /** @type {!Array<Number>} */ (attachedFile.fileData.bytes));
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
        getElement('#userConsentCheckbox').checked;
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
        getElement('#userConsentCheckbox').checked = true;
        await flushTasks();

        const reportWithConsent = (await clickSendAndWait(page)).report;

        assertTrue(reportWithConsent.contactUserConsentGranted);

        page.reEnableSendReportButton();
        page.feedbackContext = fakeFeedbackContext;
        getElement('#userConsentCheckbox').checked = false;
        await flushTasks();

        const reportWithoutConsent = (await clickSendAndWait(page)).report;
        assertFalse(reportWithoutConsent.contactUserConsentGranted);
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

        const consentLabel = getElement('#userConsentLabel');
        const consentCheckbox = getElement('#userConsentCheckbox');
        const emailDropdown = getElement('#userEmailDropDown');

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

    const reportWithoutExtraDiagnostics = (await clickSendAndWait(page)).report;
    assertFalse(
        !!reportWithoutExtraDiagnostics.feedbackContext.extraDiagnostics);

    page.reEnableSendReportButton();
    const fakeFeedbackContextWithExtraDiagnostics =
        /** @type {!FeedbackContext} */ ({extraDiagnostics: 'some extra info'});
    page.feedbackContext = fakeFeedbackContextWithExtraDiagnostics;
    await flushTasks();

    const reportWithExtraDiagnostics = (await clickSendAndWait(page)).report;
    assertEquals(
        fakeFeedbackContextWithExtraDiagnostics.extraDiagnostics,
        reportWithExtraDiagnostics.feedbackContext.extraDiagnostics);

    getElement('#sysInfoCheckbox').checked = false;
    page.reEnableSendReportButton();
    const reportNoSysInfo = (await clickSendAndWait(page)).report;
    assertFalse(!!reportNoSysInfo.feedbackContext.extraDiagnostics);
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
    const linkCrossDeviceDogfoodFeedbackCheckbox =
        getElement('#linkCrossDeviceDogfoodFeedbackCheckbox');
    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
    assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

    // Uncheck the bluetooth logs checkbox.
    const bluetoothLogsCheckbox = getElement('#bluetoothLogsCheckbox');
    assertTrue(!!bluetoothLogsCheckbox);
    bluetoothLogsCheckbox.checked = false;
    assertFalse(bluetoothLogsCheckbox.checked);

    const reportWithoutCategoryTag = (await clickSendAndWait(page)).report;
    assertFalse(!!reportWithoutCategoryTag.feedbackContext.categoryTag);

    page.reEnableSendReportButton();
    const fakeFeedbackContextWithCategoryTag =
        /** @type {!FeedbackContext} */ ({categoryTag: 'some category tag'});
    page.feedbackContext = fakeFeedbackContextWithCategoryTag;
    await flushTasks();

    const reportWithCategoryTag = (await clickSendAndWait(page)).report;
    assertEquals(
        fakeFeedbackContextWithCategoryTag.categoryTag,
        reportWithCategoryTag.feedbackContext.categoryTag);

    // Check the bluetooth logs checkbox. The categoryTag
    // should be BluetoothReportWithLogs, not the tag from url.
    page.reEnableSendReportButton();
    bluetoothLogsCheckbox.checked = true;
    assertTrue(bluetoothLogsCheckbox.checked);
    assertTrue(isVisible(bluetoothLogsCheckbox));
    await flushTasks();

    const reportWithCategoryTagAndBluetoothFlag =
        (await clickSendAndWait(page)).report;
    assertEquals(
        'BluetoothReportWithLogs',
        reportWithCategoryTagAndBluetoothFlag.feedbackContext.categoryTag);
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
        const bluetoothLogsCheckbox = getElement('#bluetoothLogsCheckbox');
        assertTrue(!!bluetoothLogsCheckbox);
        bluetoothLogsCheckbox.checked = false;
        assertFalse(bluetoothLogsCheckbox.checked);

        // Uncheck the "Link Cross Device Dogfood Feedback" checkbox.
        const linkCrossDeviceDogfoodFeedbackCheckbox =
            getElement('#linkCrossDeviceDogfoodFeedbackCheckbox');
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
        assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

        const reportWithoutCategoryTag = (await clickSendAndWait(page)).report;
        assertFalse(!!reportWithoutCategoryTag.feedbackContext.categoryTag);

        page.reEnableSendReportButton();
        const fakeFeedbackContextWithCategoryTag =
            /** @type {!FeedbackContext} */ (
                {categoryTag: 'some category tag'});
        page.feedbackContext = fakeFeedbackContextWithCategoryTag;
        await flushTasks();

        const reportWithCategoryTag = (await clickSendAndWait(page)).report;
        assertEquals(
            fakeFeedbackContextWithCategoryTag.categoryTag,
            reportWithCategoryTag.feedbackContext.categoryTag);

        // Check the Link Cross Device Dogfood Feedback checkbox. The
        // categoryTag should be
        // 'linkCrossDeviceDogfoodFeedbackWithoutBluetoothLogs'.
        page.reEnableSendReportButton();
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        linkCrossDeviceDogfoodFeedbackCheckbox.checked = true;
        assertTrue(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

        page.reEnableSendReportButton();

        const reportWithCrossDeviceWithoutBluetoothLogsTag =
            (await clickSendAndWait(page)).report;
        assertEquals(
            'linkCrossDeviceDogfoodFeedbackWithoutBluetoothLogs',
            reportWithCrossDeviceWithoutBluetoothLogsTag.feedbackContext
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
        const linkCrossDeviceDogfoodFeedbackCheckbox =
            getElement('#linkCrossDeviceDogfoodFeedbackCheckbox');
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        linkCrossDeviceDogfoodFeedbackCheckbox.checked = true;
        assertTrue(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

        const bluetoothLogsCheckbox = getElement('#bluetoothLogsCheckbox');
        assertTrue(!!bluetoothLogsCheckbox);
        bluetoothLogsCheckbox.checked = true;
        assertTrue(bluetoothLogsCheckbox.checked);

        await flushTasks();

        const reportWithCrossDeviceWithBluetoothLogsTag =
            (await clickSendAndWait(page)).report;
        assertEquals(
            'linkCrossDeviceDogfoodFeedbackWithBluetoothLogs',
            reportWithCrossDeviceWithBluetoothLogsTag.feedbackContext
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

    getElement('#histogramsLink').click();

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

    getElement('#sysInfoLink').click();

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

    getElement('#autofillMetadataUrl').click();

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
    const closeDialogButton = getElement('#bluetoothDialogDoneButton');
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #bluetoothLogsLink, the dialog pops up.
    getElement('#bluetoothLogsInfoLink').click();
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
    const closeDialogButton =
        getElement('#linkCrossDeviceDogfoodFeedbackDialogDoneButton');
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #linkCrossDeviceDogfoodFeedbackLink, the dialog pops
    // up.
    getElement('#linkCrossDeviceDogfoodFeedbackInfoLink').click();
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
    const closeDialogButton = getElement('#assistantDialogDoneButton');
    assertFalse(isVisible(closeDialogButton));

    // After clicking the #bluetoothLogsLink, the dialog pops up.
    getElement('#assistantLogsLink').click();
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
        getElement('#userEmailDropDown').value, 'test.user@google.com');
    getElement('#bluetoothCheckboxContainer').hidden = false;

    const bluetoothLogsCheckbox = getElement('#bluetoothLogsCheckbox');
    const linkCrossDeviceDogfoodFeedbackCheckbox =
        getElement('#linkCrossDeviceDogfoodFeedbackCheckbox');

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
    const requestWithBluetoothFlag = (await clickSendAndWait(page)).report;

    assertTrue(requestWithBluetoothFlag.sendBluetoothLogs);
    assertTrue(!!requestWithBluetoothFlag.feedbackContext.categoryTag);
    assertEquals(
        'BluetoothReportWithLogs',
        requestWithBluetoothFlag.feedbackContext.categoryTag);
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
        getElement('#userEmailDropDown').value, 'test.user@google.com');
    getElement('#bluetoothCheckboxContainer').hidden = false;

    // BluetoothLogs checkbox is default to be checked.
    const bluetoothLogsCheckbox = getElement('#bluetoothLogsCheckbox');
    assertTrue(!!bluetoothLogsCheckbox);
    assertTrue(bluetoothLogsCheckbox.checked);

    // Uncheck the "Link Cross Device Dogfood Feedback" checkbox so that only
    // the Bluetooth-specific categoryTag is added to the report.
    const linkCrossDeviceDogfoodFeedbackCheckbox =
        getElement('#linkCrossDeviceDogfoodFeedbackCheckbox');
    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    linkCrossDeviceDogfoodFeedbackCheckbox.checked = false;
    assertFalse(linkCrossDeviceDogfoodFeedbackCheckbox.checked);

    // Verify that unchecking the checkbox will remove the flag in the report.
    bluetoothLogsCheckbox.click();
    assertFalse(bluetoothLogsCheckbox.checked);
    await flushTasks();

    // Report should not have sendBluetoothLogs flag,
    // and category should not be marked as "BluetoothReportWithLogs".
    const requestWithoutBluetoothFlag = (await clickSendAndWait(page)).report;

    assertFalse(requestWithoutBluetoothFlag.sendBluetoothLogs);
    assertFalse(!!requestWithoutBluetoothFlag.feedbackContext.categoryTag);
  });
}
