// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeFeedbackContext, fakePngData, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {AdditionalContextQueryParam, FeedbackFlowElement, FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {FeedbackContext, SendReportStatus} from 'chrome://os-feedback/feedback_types.js';
import {setFeedbackServiceProviderForTesting, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {SearchPageElement} from 'chrome://os-feedback/search_page.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {eventToPromise, flushTasks} from '../../test_util.js';

export function FeedbackFlowTestSuite() {
  /** @type {?FeedbackFlowElement} */
  let page = null;

  /** @type {?FakeHelpContentProvider} */
  let helpContentProvider = null;

  /** @type {?FakeFeedbackServiceProvider} */
  let feedbackServiceProvider = null;

  setup(() => {
    document.body.innerHTML = '';
    // Create helpContentProvider.
    helpContentProvider = new FakeHelpContentProvider();
    // Setup search response.
    helpContentProvider.setFakeSearchResponse(fakeSearchResponse);
    // Set the fake helpContentProvider.
    setHelpContentProviderForTesting(helpContentProvider);

    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    feedbackServiceProvider.setFakeFeedbackContext(fakeFeedbackContext);
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  });

  teardown(() => {
    page.remove();
    page = null;
    helpContentProvider = null;
    feedbackServiceProvider = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!FeedbackFlowElement} */ (
            document.createElement('feedback-flow'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /**
   * @suppress {visibility}
   * @return {?FeedbackContext}
   */
  function getFeedbackContext_() {
    assertTrue(!!page);

    return page.feedbackContext_;
  }

  /** @return {!SearchPageElement} */
  function getSearchPage() {
    assertTrue(!!page);

    return /** @type {!SearchPageElement} */ (page.$['searchPage']);
  }

  // Test that the search page is shown by default.
  test('SearchPageIsShownByDefault', async () => {
    await initializePage();

    // Find the element whose class is iron-selected.
    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // Verify the title is in the page.
    const title = activePage.shadowRoot.querySelector('.page-title');
    assertTrue(!!title);
    assertEquals('Send feedback', title.textContent.trim());

    // Verify the continue button is in the page.
    const buttonContinue =
        activePage.shadowRoot.querySelector('#buttonContinue');
    assertTrue(!!buttonContinue);
    assertEquals('Continue', buttonContinue.textContent.trim());
  });


  // Test that the share data page is shown.
  test('ShareDataPageIsShown', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);

    assertTrue(!!activePage);
    // Verify the title is in the page.
    const title = activePage.shadowRoot.querySelector('.page-title');
    assertTrue(!!title);
    assertEquals('Send feedback', title.textContent.trim());

    // Verify the back button is in the page.
    const buttonBack = activePage.shadowRoot.querySelector('#buttonBack');
    assertTrue(!!buttonBack);
    assertEquals('Back', buttonBack.textContent.trim());

    // Verify the send button is in the page.
    const buttonSend = activePage.shadowRoot.querySelector('#buttonSend');
    assertTrue(!!buttonSend);
    assertEquals('Send', buttonSend.textContent.trim());
  });


  // Test that the confirmation page is shown.
  test('ConfirmationPageIsShown', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.CONFIRMATION);
    page.setSendReportStatusForTesting(SendReportStatus.kSuccess);

    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('confirmationPage', activePage.id);

    // Verify the title is in the page.
    const title = activePage.shadowRoot.querySelector('.page-title');
    assertTrue(!!title);
    assertEquals('Thanks for your feedback', title.textContent.trim());

    // Verify the done button is in the page.
    const buttonDone = activePage.shadowRoot.querySelector('#buttonDone');
    assertTrue(!!buttonDone);
    assertEquals('Done', buttonDone.textContent.trim());

    // Verify the startNewReport button is in the page.
    const buttonNewReport =
        activePage.shadowRoot.querySelector('#buttonNewReport');
    assertTrue(!!buttonNewReport);
    assertEquals('Send new report', buttonNewReport.textContent.trim());
  });

  // Test the navigation from search page to share data page.
  test('NavigateFromSearchPageToShareDataPage', async () => {
    await initializePage();

    let activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    const inputElement = activePage.shadowRoot.querySelector('textarea');
    const continueButton =
        activePage.shadowRoot.querySelector('#buttonContinue');

    // Clear the description.
    inputElement.value = '';
    continueButton.click();
    await flushTasks();
    // Should stay on search page when click the continue button.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('searchPage', activePage.id);
    assertEquals(0, feedbackServiceProvider.getScreenshotPngCallCount());
    feedbackServiceProvider.setFakeScreenshotPng(fakePngData);

    const clickPromise = eventToPromise('continue-click', page);

    let eventDetail;
    page.addEventListener('continue-click', (event) => {
      eventDetail = event.detail;
    });

    // Enter some text.
    inputElement.value = 'abc';
    continueButton.click();
    await clickPromise;

    assertEquals(FeedbackFlowState.SEARCH, eventDetail.currentState);
    assertEquals('abc', eventDetail.description);

    // Should move to share data page when click the continue button.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);

    // Verify that the getScreenshotPng is called once.
    assertEquals(1, feedbackServiceProvider.getScreenshotPngCallCount());
    const screenshotImg =
        activePage.shadowRoot.querySelector('#screenshotImage');
    assertTrue(!!screenshotImg);
    assertTrue(!!screenshotImg.src);
    // Verify that the src of the screenshot image is set.
    assertTrue(screenshotImg.src.startsWith('blob:chrome://os-feedback/'));
  });

  // Test the navigation from share data page back to search page when click
  // the back button.
  test('NavigateFromShareDataPageToSearchPage', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    let activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);

    activePage.shadowRoot.querySelector('#buttonBack').click();
    await flushTasks();
    // Should go back to share data page.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // The description input element should have received focused.
    const descriptionElement = activePage.shadowRoot.querySelector('textarea');
    assertEquals(descriptionElement, getDeepActiveElement());
  });

  // Test the navigation from share data page to confirmation page after the
  // send button is clicked.
  test('NavigateFromShareDataPageToConfirmationPage', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);
    // In normal flow, the description should have been set when arriving to the
    // share data page.
    page.setDescriptionForTesting('abc123');

    let activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);

    const clickPromise = eventToPromise('continue-click', page);

    let eventDetail;
    page.addEventListener('continue-click', (event) => {
      eventDetail = event.detail;
    });

    assertEquals(0, feedbackServiceProvider.getSendReportCallCount());

    activePage.shadowRoot.querySelector('#buttonSend').click();
    await clickPromise;

    // Verify the sendReport method was invoked.
    assertEquals(1, feedbackServiceProvider.getSendReportCallCount());
    assertEquals(FeedbackFlowState.SHARE_DATA, eventDetail.currentState);

    // Should navigate to confirmation page.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('confirmationPage', activePage.id);
    assertEquals(SendReportStatus.kSuccess, activePage.sendReportStatus);
  });

  // Test the navigation from confirmation page to search page after the
  // send new report button is clicked.
  test('NavigateFromConfirmationPageToSearchPage', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.CONFIRMATION);
    // Set text input in search page for testing.
    const searchPage = page.shadowRoot.querySelector('#searchPage');
    searchPage.setDescription(/*text=*/ 'abc123');

    let activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('confirmationPage', activePage.id);

    const clickPromise = eventToPromise('go-back-click', page);

    let eventDetail;
    page.addEventListener('go-back-click', (event) => {
      eventDetail = event.detail;
    });

    activePage.shadowRoot.querySelector('#buttonNewReport').click();
    await clickPromise;

    assertEquals(FeedbackFlowState.CONFIRMATION, eventDetail.currentState);

    // Should navigate to search page.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // Search text should be empty.
    const inputElement =
        searchPage.shadowRoot.querySelector('#descriptionText');
    assertEquals(inputElement.value, '');
    // The description input element should have received focused.
    assertEquals(inputElement, getDeepActiveElement());
  });

  // When starting a new report, the send button in share data page
  // should be re-enabled.
  test('SendNewReportShouldEnableSendButton', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);
    // In normal flow, the description should have been set when arriving to the
    // share data page.
    page.setDescriptionForTesting('abc123');

    const continueClickPromise = eventToPromise('continue-click', page);
    const goBackClickPromise = eventToPromise('go-back-click', page);

    let activePage = page.shadowRoot.querySelector('.iron-selected');
    const shareDataPageSendButton =
        activePage.shadowRoot.querySelector('#buttonSend');
    activePage.shadowRoot.querySelector('#buttonSend').click();
    await continueClickPromise;

    // Should navigate to confirmation page.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('confirmationPage', activePage.id);

    // The send button in the share data page should be disabled after
    // sending the report and before send new report button is clicked
    assertTrue(shareDataPageSendButton.disabled);

    // Click send new report button.
    activePage.shadowRoot.querySelector('#buttonNewReport').click();
    await goBackClickPromise;

    // Should navigate to search page.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // Add some text and clicks continue button.
    activePage.setDescription(/*text=*/ 'abc123');
    activePage.shadowRoot.querySelector('#buttonContinue').click();
    await continueClickPromise;

    // Should navigate to share data page.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('shareDataPage', activePage.id);

    // The send button in the share data page should be re-enabled.
    assertFalse(shareDataPageSendButton.disabled);
  });

  // Test that the getUserEmail is called after initialization.
  test('GetUserEmailIsCalled', async () => {
    assertEquals(0, feedbackServiceProvider.getFeedbackContextCallCount());
    await initializePage();
    assertEquals(1, feedbackServiceProvider.getFeedbackContextCallCount());
  });

  // Test that the extra diagnostics gets set when query parameter is non-empty.
  test(
      'AdditionalContextParametersProvidedInUrl_FeedbackContext_Matches',
      async () => {
        const queryParams = new URLSearchParams(window.location.search);
        const extra_diagnostics = 'some%20extra%20diagnostics';
        queryParams.set(
            AdditionalContextQueryParam.EXTRA_DIAGNOSTICS, extra_diagnostics);
        const description_template = 'Q1%3A%20Question%20one?';
        queryParams.set(
            AdditionalContextQueryParam.DESCRIPTION_TEMPLATE,
            description_template);
        // Replace current querystring with the new one.
        window.history.replaceState(null, '', '?' + queryParams.toString());
        await initializePage();
        page.setCurrentStateForTesting(FeedbackFlowState.SEARCH);
        const descriptionElement = getSearchPage().$['descriptionText'];

        const feedbackContext = getFeedbackContext_();
        assertEquals(fakeFeedbackContext.pageUrl, feedbackContext.pageUrl);
        assertEquals(fakeFeedbackContext.email, feedbackContext.email);
        assertEquals(
            decodeURIComponent(extra_diagnostics),
            feedbackContext.extraDiagnostics);
        assertEquals(
            decodeURIComponent(description_template), descriptionElement.value);
      });

  // Test that the extra diagnostics gets set when query parameter is empty.
  test(
      'AdditionalContextParametersNotProvidedInUrl_FeedbackContext_UsesDefault',
      async () => {
        // Replace current querystring with the new one.
        window.history.replaceState(
            null, '',
            '?' +
                '');
        await initializePage();
        page.setCurrentStateForTesting(FeedbackFlowState.SEARCH);
        const descriptionElement = getSearchPage().$['descriptionText'];

        const feedbackContext = getFeedbackContext_();
        // TODO(ashleydp): Update expectation when page_url passed.
        assertEquals(fakeFeedbackContext.pageUrl, feedbackContext.pageUrl);
        assertEquals(fakeFeedbackContext.email, feedbackContext.email);
        assertEquals('', feedbackContext.extraDiagnostics);
        assertEquals('', descriptionElement.value);
      });
}
