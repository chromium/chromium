// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeEmptyFeedbackContext, fakeFeedbackContext} from 'chrome://os-feedback/fake_data.js';
import {ShareDataPageElement} from 'chrome://os-feedback/share_data_page.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

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
    assertTrue(!!getElement('#screenshot-checkbox'));
    assertEquals('Screenshot', getElementContent('#screenshot-check-label'));
    assertTrue(!!getElement('#screenshot-image'));

    // Add file element.
    assertEquals('Add file', getElementContent('#add-file'));

    // Email elements.
    assertEquals('Email', getElementContent('#user-email-label'));
    assertTrue(!!getElement('#user-email-drop-down'));

    // URL elements.
    assertEquals('share url:', getElementContent('#page-url-label'));
    assertTrue(!!getElement('#page-url-checkbox'));
    assertTrue(!!getElement('#page-url-text'));

    // System info label is a localized string in HTML format.
    assertTrue(getElementContent('#sys-info-label').length > 0);

    // Privacy note is a long localized string in HTML format.
    assertTrue(getElementContent('#privacy-note').length > 0);
  });

  // Test that the email drop down is populated with two options.
  test('emailDropdownPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    const emailDropdown = getElement('#user-email-drop-down');
    assertTrue(!!emailDropdown);
    assertEquals(2, emailDropdown.options.length);

    const firstOption = emailDropdown.options.item(0);
    assertEquals('test.user2@test.com', firstOption.textContent);
    assertEquals('test.user2@test.com', firstOption.value);

    const secondOption = emailDropdown.options.item(1);
    assertEquals('Don\'t include email address', secondOption.textContent);
    assertEquals('', secondOption.value);

    // The user email section should be visible.
    const userEmailElement = getElement('#user-email');
    assertTrue(!!userEmailElement);
    assertTrue(isVisible(userEmailElement));
  });

  // Test that the email section is hidden when there is no email.
  test('emailSectionHiddenWithoutEmail', async () => {
    await initializePage();
    page.feedbackContext = fakeEmptyFeedbackContext;

    // The user email section should be hidden.
    const userEmailElement = getElement('#user-email');
    assertTrue(!!userEmailElement);
    assertFalse(isVisible(userEmailElement));
  });

  test('pageUrlPopulated', async () => {
    await initializePage();
    page.feedbackContext = fakeFeedbackContext;

    assertEquals('chrome://tab/', getElement('#page-url-text').value);
  });
}
