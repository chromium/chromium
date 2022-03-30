// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {FeedbackFlowElement, FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function FeedbackFlowTestSuite() {
  /** @type {?FeedbackFlowElement} */
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
    page.currentState_ = FeedbackFlowState.SHARE_DATA;

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
    page.currentState_ = FeedbackFlowState.CONFIRMATION;

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
}
