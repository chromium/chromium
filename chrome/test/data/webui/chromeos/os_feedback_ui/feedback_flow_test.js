// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeFeedbackContext, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {FeedbackFlowElement, FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {setFeedbackServiceProviderForTesting, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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

  // Test that the search page is shown by default.
  test('SearchPageIsShownByDefault', async () => {
    await initializePage();

    // Find the element whose class is iron-selected.
    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // Verify the title is in the page.
    const title = activePage.shadowRoot.querySelector('#title');
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
    const title = activePage.shadowRoot.querySelector('#title');
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

    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('confirmationPage', activePage.id);

    // Verify the title is in the page.
    const title = activePage.shadowRoot.querySelector('#title');
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

    // Enter some text.
    inputElement.value = 'abc';
    continueButton.click();

    await flushTasks();
    // Should move to share data page when click the continue button.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);
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
  });

  // Test that the getUserEmail is called after initialization.
  test('GetUserEmailIsCalled', async () => {
    assertEquals(0, feedbackServiceProvider.getFeedbackContextCallCount());
    await initializePage();
    assertEquals(1, feedbackServiceProvider.getFeedbackContextCallCount());
  });
}
