// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeEmptySearchResponse, fakeFeedbackContext, fakeInternalUserFeedbackContext, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {SearchResponse} from 'chrome://os-feedback/feedback_types.js';
import {setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {domainQuestions, questionnaireBegin} from 'chrome://os-feedback/questionnaire.js';
import {OS_FEEDBACK_UNTRUSTED_ORIGIN, SearchPageElement} from 'chrome://os-feedback/search_page.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise, isVisible} from '../test_util.js';

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

    // Fire search immediately for input change.
    page.searchTimoutInMs_ = 0;

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
   * - Case 3: When the text area is empty, search is NOT triggered.
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
    assertTrue(page.getIsPopularContentForTesting_());

    // Enter three chars.
    textAreaElement.value = 'abc';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc'.
    assertEquals('abc', provider.lastQuery);
    assertFalse(page.getIsPopularContentForTesting_());

    // Enter 2 more characters. This should trigger another search.
    textAreaElement.value = 'abc12';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query
    // 'abc12'.
    assertEquals('abc12', provider.lastQuery);

    // Fire search after pausing typing for 10 seconds.
    page.searchTimoutInMs_ = 10000;
    // Remove some chars. This should NOT trigger another search.
    textAreaElement.value = 'a';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has NOT been called with query
    // 'a'.
    assertNotEquals('a', provider.lastQuery);

    // Fire search immediately for input change.
    page.searchTimoutInMs_ = 0;

    // Enter one more characters. This should trigger another search.
    textAreaElement.value = 'abc123';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc123'.
    assertEquals('abc123', provider.lastQuery);
    assertFalse(page.getIsPopularContentForTesting_());

    // Remove all the text area characters. This should NOT trigger
    // getHelpContent().
    textAreaElement.value = '';
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Verify that getHelpContent() is not called, and the help content
    // is the default popular content.
    assertNotEquals('', provider.lastQuery);
    assertTrue(page.getIsPopularContentForTesting_());
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
        'on a Chromebook?';
    textAreaElement.value = longTextNoResult;
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Verify that getHelpContent() has been called with query
    // longTextNoSearchResult.
    assertEquals(longTextNoResult, provider.lastQuery);
    // Search result count should be 0.
    assertEquals(0, page.getSearchResultCountForTesting());
    // Popular content should be displayed (i.e. isPopularContent = true).
    assertTrue(page.getIsPopularContentForTesting_());
  });

  /**
   * Test that popular help content is displayed when no match is returned after
   * filtering but there were matches.
   */
  test('NoItemsReturnedButThereWereMatches', async () => {
    /** {?Element} */
    let textAreaElement = null;

    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    textAreaElement = getElement('#descriptionText');

    /** @type {!SearchResponse} */
    const fakeResponse = {
      results: [],  // None items returned after filtering out other languages.
      totalResults: 15,  // 15 matches.
    };

    provider.setFakeSearchResponse(fakeResponse);
    textAreaElement.value = 'abc';

    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc'.
    assertEquals('abc', provider.lastQuery);
    // Search result count should be 0.
    assertEquals(0, page.getSearchResultCountForTesting());
    // Popular content should be displayed (i.e. isPopularContent = true).
    assertTrue(page.getIsPopularContentForTesting_());
  });

  /**
   * Test that if an older query came back later than its next query, then its
   * results are ignored.
   */
  test('IgnoreOutOfOrderSearchResults', async () => {
    /** {?Element} */
    let textAreaElement = null;

    await initializePage();
    page.feedbackContext = fakeFeedbackContext;
    textAreaElement = getElement('#descriptionText');
    await flushTasks();

    const iframe = /** @type {!HTMLIFrameElement} */ (getElement('iframe'));
    assertTrue(!!iframe);
    // Wait for the iframe completes loading.
    await eventToPromise('load', iframe);

    // The query seq no starts from 0. When page is initialized, it will fire a
    // query with empty text.
    assertEquals(1, page.getNextQuerySeqNoForTesting());
    assertEquals(0, page.getLastPostedQuerySeqNoForTesting());

    // Enter some chars.
    textAreaElement.value = 'abc';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Verify that query seq no has been incremented by 1.
    assertEquals(2, page.getNextQuerySeqNoForTesting());
    // Verify that last posted query seq no has been updated correctly.
    assertEquals(1, page.getLastPostedQuerySeqNoForTesting());

    // Reset the next query seq no to 0 (< 1) to simulate the out of order case.
    // The result should be ignored.
    page.setNextQuerySeqNoForTesting(0);
    assertEquals(0, page.getNextQuerySeqNoForTesting());
    assertEquals(1, page.getLastPostedQuerySeqNoForTesting());

    // Update the text.
    textAreaElement.value = 'a';
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Verify that query seq no has been incremented by 1.
    assertEquals(1, page.getNextQuerySeqNoForTesting());
    // Verify that the last posted query sequence no was not updated to 0. This
    // means the search results are ignored.
    assertEquals(1, page.getLastPostedQuerySeqNoForTesting());
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

    // There is another message posted from iframe which sends the height of
    // the help content.
    const expectedMessageEventCount = 2;
    let messageEventCount = 0;
    const resolver = new PromiseResolver();

    window.addEventListener('message', (event) => {
      if (OS_FEEDBACK_UNTRUSTED_ORIGIN === event.origin &&
          'help-content-received-for-testing' === event.data.id) {
        helpContentReceived = true;
        helpContentCountReceived = event.data.count;
      }
      messageEventCount++;
      if (messageEventCount === expectedMessageEventCount) {
        resolver.resolve();
      }
    });

    const data = {
      contentList: fakeSearchResponse.results,
      isQueryEmpty: true,
      isPopularContent: true,
    };
    iframe.contentWindow.postMessage(data, OS_FEEDBACK_UNTRUSTED_ORIGIN);

    // Wait for the "help-content-received" message has been received.
    await resolver.promise;
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
    assertFalse(isVisible(errorMsg));

    const textInput = getElement('#descriptionText');
    assertTrue(textInput.value.length === 0);
    // Remove focus on the textarea.
    textInput.blur();
    assertNotEquals(getDeepActiveElement(), textInput);

    const buttonContinue = getElement('#buttonContinue');
    buttonContinue.click();

    // Verify that the message is visible now.
    assertTrue(isVisible(errorMsg));
    assertEquals('Description is required', errorMsg.textContent.trim());
    // Verify that the textarea received focus again.
    assertEquals(getDeepActiveElement(), textInput);

    // Now enter some spaces. The error message should still be visible.
    textInput.value = '   ';
    textInput.dispatchEvent(new Event('input'));

    assertTrue(isVisible(errorMsg));

    // Now enter some text. The error message should be hidden again.
    textInput.value = 'hello';
    textInput.dispatchEvent(new Event('input'));

    assertFalse(isVisible(errorMsg));

    // Verify that all whitespace input is treated as empty.
    textInput.value = '   ';
    buttonContinue.click();
    assertTrue(isVisible(errorMsg));
    assertEquals(getDeepActiveElement(), textInput);
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

  test('typingDisplayWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My display is working great!';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire and all relevant domain questions are
    // present.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['display'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('typingScreenWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My screen is too awesome!';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire and all relevant domain questions are
    // present.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['display'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('typingWifiDisplayWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My wifi and display is working great!';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that both questionnaires and all relevant domain questions are
    // present.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['display'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
    domainQuestions['wifi'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('typingMultiDisplayWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My screen and display is working great over HDMI!';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire and all relevant domain questions are
    // present only once.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['display'].forEach((question) => {
      const idx = textAreaElement.value.indexOf(question);
      assertTrue(
          idx >= 0 && textAreaElement.value.indexOf(question, idx + 1) < 0);
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

  test('typingBluetoothTwiceOnlyPastesTheQuestionsOnce', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My cat got a blue tooth because of ChromeOS.';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here twice to simulate pressing two keys.
    textAreaElement.dispatchEvent(new Event('input'));
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that there is only one instance of the first question.
    assertEquals(
        2, textAreaElement.value.split(domainQuestions['bluetooth'][0]).length);
  });

  test('typingUsbWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'My USB port stopped working!';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire with USB questions is shown.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['usb'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('typingThunderboltWithInternalAccountShowsQuestionnaire', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'There is an issue with my Thunderbolt 3 device.';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that the questionnaire with Thunderbolt questions is shown.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['thunderbolt'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });
  });

  test('thunderboltQuestionnaireIsPrioritizedOverUsb', async () => {
    let textAreaElement = null;
    await initializePage();
    // The questionnaire will be only shown if the account belongs to an
    // internal user.
    page.feedbackContext = fakeInternalUserFeedbackContext;

    textAreaElement = getElement('#descriptionText');
    textAreaElement.value = 'The USB-C connector on my TBT4 dock is broken';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));
    await flushTasks();

    // Check that Thunderbolt questions are shown in the questionnaire.
    assertTrue(textAreaElement.value.indexOf(questionnaireBegin) >= 0);
    domainQuestions['thunderbolt'].forEach((question) => {
      assertTrue(textAreaElement.value.indexOf(question) >= 0);
    });

    // Check that USB-specific questions are not shown. Questions shared
    // between USB and Thunderbolt will be included.
    domainQuestions['usb'].forEach((question) => {
      if (domainQuestions['thunderbolt'].indexOf(question) < 0) {
        assertTrue(textAreaElement.value.indexOf(question) < 0);
      }
    });
  });
}
