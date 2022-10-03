// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeEmptySearchResponse, fakeFeedbackContext, fakeInternalUserFeedbackContext, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {domainQuestions, questionnaireBegin} from 'chrome://os-feedback/questionnaire.js';
import {OS_FEEDBACK_UNTRUSTED_ORIGIN, SearchPageElement} from 'chrome://os-feedback/search_page.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {eventToPromise} from '../../test_util.js';

export function searchPageTestSuite() {
  /** @type {?SearchPageElement} */
  let page = null;

  /** @type {?FakeHelpContentProvider} */
  let provider = null;

  setup(() => {
    document.body.innerHTML = '';
    // Create provider.
    provider = new FakeHelpContentProvider();
    // Setup search response.
    provider.setFakeSearchResponse(fakeSearchResponse);
    // Set the fake provider.
    setHelpContentProviderForTesting(provider);
  });

  teardown(() => {
    page.remove();
    page = null;
    provider = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!SearchPageElement} */ (
            document.createElement('search-page'));
    assertTrue(!!page);
    document.body.appendChild(page);

    return flushTasks();
  }

  /**
   * @param {string} selector
   * @returns {!Element}
   */
  function getElement(selector) {
    const element = page.shadowRoot.querySelector(selector);
    assertTrue(!!element);
    return /** @type {!Element} */ (element);
  }

  /**
   * Test that expected html elements are in the page after loaded.
   */
  test('SearchPageLoaded', async () => {
    await initializePage();
    // Verify the title is in the page.
    const title = page.shadowRoot.querySelector('.page-title');
    assertEquals('Send feedback', title.textContent.trim());

    // Verify the iframe is in the page.
    const untrustedFrame = getElement('iframe');
    assertEquals(
        'chrome-untrusted://os-feedback/untrusted_index.html',
        untrustedFrame.src);

    // Focus is set after the iframe is loaded.
    await eventToPromise('load', untrustedFrame);
    // Verify the description input is focused.
    assertEquals(getElement('textarea'), getDeepActiveElement());

    // Verify the descriptionTitle is in the page.
    const descriptionTitle = getElement('#descriptionTitle');
    assertEquals('Description', descriptionTitle.textContent.trim());

    // Verify the feedback writing guidance link is in the page.
    const writingGuidanceLink = getElement('#feedbackWritingGuidance');
    assertEquals(
        'Tips on writing feedback', writingGuidanceLink.textContent.trim());
    assertEquals('_blank', writingGuidanceLink.target);
    assertEquals(
        'https://support.google.com/chromebook/answer/2982029',
        writingGuidanceLink.href);

    // Verify the help content is not in the page. For security reason, help
    // contents fetched online can't be displayed in trusted context.
    const helpContent = page.shadowRoot.querySelector('help-content');
    assertFalse(!!helpContent);

    // Verify the continue button is in the page.
    const buttonContinue = getElement('#buttonContinue');
    assertEquals('Continue', buttonContinue.textContent.trim());
  });

  /**
   * Test that the text area accepts input and may fire search query to retrieve
   * help contents.
   * - Case 1: When number of characters newly entered is less than 3, search is
   *   not triggered.
   * - Case 2: When number of characters newly entered is 3 or more, search is
   *   triggered and help contents are populated.
   */
  test('HelpContentPopulated', async () => {
    /** {?Element} */
    let textAreaElement = null;

    await initializePage();
    // The 'input' event triggers a listener that checks for internal user
    // account, so we need to set up the context.
    page.feedbackContext = fakeFeedbackContext;
    textAreaElement = getElement('#descriptionText');
    // Verify the textarea is empty and hint is showing.
    assertEquals('', textAreaElement.value);
    assertEquals(
        'Share your feedback or describe your issue. ' +
            'If possible, include steps to reproduce your issue.',
        textAreaElement.placeholder);

    // Enter three chars.
    textAreaElement.value = 'abc';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc'.
    assertEquals('abc', provider.lastQuery);

    // Enter 2 more characters. This should NOT trigger another search.
    textAreaElement.value = 'abc12';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has NOT been called with query
    // 'abc12'.
    assertNotEquals('abc12', provider.lastQuery);

    // Enter one more characters. This should trigger another search.
    textAreaElement.value = 'abc123';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc123'.
    assertEquals('abc123', provider.lastQuery);
  });

  test('HelpContentSearchResultCountColdStart', async () => {
    /** {?Element} */
    let textAreaElement = null;

    await initializePage();
    // The 'input' event triggers a listener that checks for internal user
    // account, so we need to set up the context.
    page.feedbackContext = fakeFeedbackContext;
    textAreaElement = getElement('#descriptionText');
    // Verify the textarea is empty now.
    assertEquals('', textAreaElement.value);

    // Enter three chars.
    textAreaElement.value = 'abc';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc'.
    assertEquals('abc', provider.lastQuery);
    // Search result count should be 5.
    assertEquals(5, page.getSearchResultCountForTesting());

    provider.setFakeSearchResponse(fakeEmptySearchResponse);
    const longTextNoResult =
        'Whenever I try to open ANY file (MS or otherwise) I get a notice ' +
        'that says “checking to find Microsoft 365 Subscription” I have ' +
        'Office on my PC, but not on my Chromebook. How do I run Word Online ' +
        'on a Chromebook? ';
    textAreaElement.value = longTextNoResult;
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Verify that getHelpContent() has been called with query
    // longTextNoSearchResult.
    assertEquals(longTextNoResult, provider.lastQuery);
    // Search result count should be 0.
    assertEquals(0, page.getSearchResultCountForTesting());
  });

  /**
   * Test that the search page can send help content to embedded untrusted page
   * via postMessage.
   */
  test('CanCommunicateWithUntrustedPage', async () => {
    /** Whether untrusted page has received new help contents. */
    let helpContentReceived = false;
    /** Number of help contents received by untrusted page. */
    let helpContentCountReceived = 0;

    await initializePage();

    const iframe = /** @type {!HTMLIFrameElement} */ (getElement('iframe'));
    assertTrue(!!iframe);
    // Wait for the iframe completes loading.
    await eventToPromise('load', iframe);

    window.addEventListener('message', (event) => {
      if (OS_FEEDBACK_UNTRUSTED_ORIGIN === event.origin &&
          'help-content-received-for-testing' === event.data.id) {
        helpContentReceived = true;
        helpContentCountReceived = event.data.count;
      }
    });

    const data = {
      contentList: fakeSearchResponse.results,
      isQueryEmpty: true,
      isPopularContent: true,
    };
    iframe.contentWindow.postMessage(data, OS_FEEDBACK_UNTRUSTED_ORIGIN);

    // Wait for the "help-content-received" message has been received.
    await eventToPromise('message', window);
    // Verify that help contents have been received.
    assertTrue(helpContentReceived);
    // Verify that 5 help contents have been received.
    assertEquals(5, helpContentCountReceived);
  });

  /**
   * Test that when there is no text entered and the continue button is clicked,
   * the error message should be displayed below the textarea and the textarea
   * should receive focus to accept input. Once some text has been entered, the
   * error message should be hidden.
   */
  test('DescriptionEmptyError', async () => {
    await initializePage();
    // The 'input' event triggers a listener that checks for internal user
    // account, so we need to set up the context.
    page.feedbackContext = fakeFeedbackContext;

    const errorMsg = getElement('#emptyErrorContainer');
    // Verify that the error message is hidden in the beginning.
    assertTrue(errorMsg.hidden);

    const textInput = getElement('#descriptionText');
    assertTrue(textInput.value.length === 0);
    // Remove focus on the textarea.
    textInput.blur();
    assertNotEquals(getDeepActiveElement(), textInput);

    const buttonContinue = getElement('#buttonContinue');
    buttonContinue.click();
    // Verify that the message is not hidden now.
    assertFalse(errorMsg.hidden);
    assertEquals('Description is required', errorMsg.textContent.trim());

    // Verify that the textarea received focus again.
    assertEquals(getDeepActiveElement(), textInput);

    // Now enter some text. The error message should be hidden again.
    textInput.value = 'hello';
    textInput.dispatchEvent(new Event('input'));

    assertTrue(errorMsg.hidden);
  });

  /**
   * Test that when there are certain text entered and the continue button is
   * clicked, an on-continue is fired.
   */
  test('Continue', async () => {
    await initializePage();

    const textInput = getElement('#descriptionText');
    textInput.value = 'hello';

    const clickPromise =
        eventToPromise('continue-click', /** @type {!Element} */ (page));
    let actualCurrentState;

    page.addEventListener('continue-click', (event) => {
      actualCurrentState = event.detail.currentState;
    });

    const buttonContinue = getElement('#buttonContinue');
    buttonContinue.click();

    await clickPromise;
    assertTrue(!!actualCurrentState);
    assertEquals(FeedbackFlowState.SEARCH, actualCurrentState);
  });

  test('typingBluetoothWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My cat got a blue tooth because of ChromeOS.';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire and all relevant domain questions are
    // present.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['bluetooth'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('typingInternetWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'The entire Internet is down.';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire and all relevant domain questions are
    // present.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['wifi'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('typing5GWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'These 5G towers control my mind.';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire and all relevant domain questions are
    // present.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['cellular'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test(
      'typingSomethingElseWithInternalAccountDoesNotShowQuestionnaire',
      async () => {
        let textAreaElement = null;
        await initializePage();
        // The questionnaire will be only shown if the account belongs to an
        // internal user.
        page.feedbackContext = fakeInternalUserFeedbackContext;

        textAreaElement = getElement('#descriptionText');
        textAreaElement.value = 'You should just make ChromeOS better.';
        // Setting the value of the textarea in code does not trigger the
        // input event. So we trigger it here.
        textAreaElement.dispatchEvent(new Event('input'));
        await flushTasks();

        // Check that the questionnaire is not shown.
        assertFalse(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
      });

  test(
      'typingBluetoothWithoutInternalAccountDoesNotShowQuestionnaire',
      async () => {
        let textAreaElement = null;
        await initializePage();
        // The questionnaire will be only shown if the account belongs to an
        // internal user.
        page.feedbackContext = fakeFeedbackContext;

        textAreaElement = getElement('#descriptionText');
        textAreaElement.value = 'My cat got a blue tooth because of ChromeOS.';
        // Setting the value of the textarea in code does not trigger the
        // input event. So we trigger it here.
        textAreaElement.dispatchEvent(new Event('input'));
        await flushTasks();

        // Check that the questionnaire is not shown.
        assertFalse(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
      });
}
