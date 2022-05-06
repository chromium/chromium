// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeEmptyFeedbackContext, fakeFeedbackContext} from 'chrome://os-feedback/fake_data.js';
import {FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {ShareDataPageElement} from 'chrome://os-feedback/share_data_page.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {eventToPromise, flushTasks, isVisible} from '../../test_util.js';

export function shareDataPageTestSuite() {
  /** @type {?ShareDataPageElement} */
  let page = null;

  setup(() => {
    document.body.innerHTML = '';
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
    assertEquals('Send feedback', getElementContent('#title'));

    // Verify the back button is in the page.
    assertEquals('Back', getElementContent('#buttonBack'));

    // Verify the send button is in the page.
    assertEquals('Send', getElementContent('#buttonSend'));

    // Screenshot elements.
    assertTrue(!!getElement('#screenshotCheckbox'));
    assertEquals('Screenshot', getElementContent('#screenshotCheckLabel'));
    assertTrue(!!getElement('#screenshotImage'));

    // Add file element.
    assertEquals('Add file', getElementContent('#addFile'));

    // Email elements.
    assertEquals('Email', getElementContent('#userEmailLabel'));
    assertTrue(!!getElement('#userEmailDropDown'));

    // URL elements.
    assertEquals('share url:', getElementContent('#pageUrlLabel'));
    assertTrue(!!getElement('#pageUrlCheckbox'));
    assertTrue(!!getElement('#pageUrlText'));

    // System info label is a localized string in HTML format.
    assertTrue(getElementContent('#sysInfoLabel').length > 0);

    // Privacy note is a long localized string in HTML format.
    assertTrue(getElementContent('#privacyNote').length > 0);
  });

  // Test that the email drop down is populated with two options.
  test('emailDropdownPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const emailDropdown = getElement('#userEmailDropDown');
    assertTrue(!!emailDropdown);
    assertEquals(2, emailDropdown.options.length);

    const firstOption = emailDropdown.options.item(0);
    assertEquals('test.user2@test.com', firstOption.textContent);
    assertEquals('test.user2@test.com', firstOption.value);

    const secondOption = emailDropdown.options.item(1);
    assertEquals('Don\'t include email address', secondOption.textContent);
    assertEquals('', secondOption.value);

    // The user email section should be visible.
    const userEmailElement = getElement('#userEmail');
    assertTrue(!!userEmailElement);
    assertTrue(isVisible(userEmailElement));
  });

  // Test that the email section is hidden when there is no email.
  test('emailSectionHiddenWithoutEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeEmptyFeedbackContext;

    // The user email section should be hidden.
    const userEmailElement = getElement('#userEmail');
    assertTrue(!!userEmailElement);
    assertFalse(isVisible(userEmailElement));
  });

  test('pageUrlPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    assertEquals('chrome://tab/', getElement('#pageUrlText').value);
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
   * Case 3: Share email.
   */
  test('SendReportShareEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    // Select the email.
    getElement('#userEmailDropDown').value = 'test.user2@test.com';

    const request = (await clickSendAndWait(page)).report;

    assertEquals('test.user2@test.com', request.feedbackContext.email);
  });

  /**
   * Test that when when the send button is clicked, an on-continue is fired.
   * Case 3: Do not share email.
   */
  test('SendReportDoNotShareEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    // Select the "Don't include email address" option.
    getElement('#userEmailDropDown').value = '';

    const request = (await clickSendAndWait(page)).report;

    assertFalse(!!request.feedbackContext.email);
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
}
