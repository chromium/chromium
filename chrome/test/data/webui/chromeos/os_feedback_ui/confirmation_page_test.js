// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConfirmationPageElement} from 'chrome://os-feedback/confirmation_page.js';
import {SendReportStatus} from 'chrome://os-feedback/feedback_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

/** @type {string} */
const ONLINE_TITLE = 'Thanks for your feedback';
/** @type {string} */
const OFFLINE_TITLE = 'You are offline now. Feedback will be sent later.';

/** @type {string} */
const ONLINE_MESSAGE =
    'Your feedback helps improve ChromeOS and will be reviewed by ' +
    'our team. Because of the large number of reports, we won\’t be able ' +
    ' to send a reply.';
/** @type {string} */
const OFFLINE_MESSAGE =
    'Thanks for the feedback. Your feedback helps improve Chrome OS ' +
    'and will be reviewed by the Chrome OS team. Because of the number ' +
    ' of reports submitted, you won’t receive a direct reply.';

export function confirmationPageTest() {
  /** @type {?ConfirmationPageElement} */
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
    page = /** @type {!ConfirmationPageElement} */ (
        document.createElement('confirmation-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /**
   * @param {?Element} host
   * @param {string} selector
   * @returns {!Element}
   */
  function getElement(host, selector) {
    const element = host.shadowRoot.querySelector(selector);
    assertTrue(!!element);
    return element;
  }

  /**
   * @param {?Element} host
   * @param {string} selector
   * @returns {string}
   */
  function getElementContent(host, selector) {
    const element = getElement(host, selector);
    return element.textContent.trim();
  }

  /**
   * @param {boolean} isOnline
   * @private
   */
  function verifyElementsByStatus(isOnline) {
    if (isOnline) {
      assertEquals(ONLINE_TITLE, getElementContent(page, '#title'));
      assertEquals(ONLINE_MESSAGE, getElementContent(page, '#message'));
    } else {
      assertEquals(OFFLINE_TITLE, getElementContent(page, '#title'));
      assertEquals(OFFLINE_MESSAGE, getElementContent(page, '#message'));
    }

    // verify help resources exist
    const helpResourcesSection = getElement(page, '#helpResources');
    assertEquals(
        'Here are some other helpful resources:',
        getElementContent(page, '#helpResourcesLabel'));
    const helpLinks = helpResourcesSection.querySelectorAll('cr-link-row');
    assertTrue(!!helpLinks);
    assertEquals(3, helpLinks.length);

    // Verify the explore app link.
    const exploreLink = helpLinks[0];
    assertTrue(isVisible(exploreLink));
    assertEquals(
        'help-resources:explore', getElement(exploreLink, '#startIcon').icon);
    assertEquals('Explore app', getElementContent(exploreLink, '#label'));
    assertEquals(
        'Find help articles and answers to common Chromebook questions',
        getElementContent(exploreLink, '#subLabel'));

    // Verify the diagnostics app link.
    const diagnosticsLink = helpLinks[1];
    assertTrue(isVisible(diagnosticsLink));
    assertEquals(
        'help-resources:diagnostics',
        getElement(diagnosticsLink, '#startIcon').icon);
    assertEquals(
        'Diagnostics app', getElementContent(diagnosticsLink, '#label'));
    assertEquals(
        'Run tests and troubleshooting for hardware issues',
        getElementContent(diagnosticsLink, '#subLabel'));

    // Verify the community link.
    const communityLink = helpLinks[2];
    if (isOnline) {
      assertTrue(isVisible(communityLink));
    } else {
      assertFalse(isVisible(communityLink));
    }
    assertEquals(
        'help-resources2:chromebook-community',
        getElement(communityLink, '#startIcon').icon);
    assertEquals(
        'Chromebook community', getElementContent(communityLink, '#label'));
    assertEquals(
        'Ask the experts in the Chromebook help forum',
        getElementContent(communityLink, '#subLabel'));

    // Verify buttons.
    assertEquals(
        'Send new report', getElementContent(page, '#buttonNewReport'));
    assertEquals('Done', getElementContent(page, '#buttonDone'));
  }

  /**TODO(xiangdongkong): test user actions */

  // Test when send report status is success, the corresponding title and
  // message are being used. The community link should be visible,
  test('onlineModeStatusSuccess', async () => {
    await initializePage();

    page.sendReportStatus = SendReportStatus.kSuccess;
    verifyElementsByStatus(/**isOnline=*/ true);
  });

  // Test when send report status is unknown, the corresponding title and
  // message are being used. The community link should be visible,
  test('offlineModeStatusUnknown', async () => {
    await initializePage();

    page.sendReportStatus = SendReportStatus.kUnknown;
    verifyElementsByStatus(/**isOnline=*/ true);
  });

  // Test when send report status is offline, the corresponding title and
  // message are being used. The community link should be invisible,
  test('offlineModeStatusDelayed', async () => {
    await initializePage();

    page.sendReportStatus = SendReportStatus.kDelayed;
    verifyElementsByStatus(/**isOnline=*/ false);
  });
}
