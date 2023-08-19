// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConfirmationPageElement} from 'chrome://os-feedback/confirmation_page.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {FeedbackAppPostSubmitAction, SendReportStatus} from 'chrome://os-feedback/feedback_types.js';
import {setFeedbackServiceProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise, isVisible} from '../test_util.js';

/** @type {string} */
const ONLINE_TITLE = 'Thanks for your feedback';
/** @type {string} */
const OFFLINE_TITLE = 'You\'re offline. Feedback will be sent later.';

/** @type {string} */
const ONLINE_MESSAGE =
    'Your feedback helps us improve the Chromebook experience and will be ' +
    'reviewed by our team. Because of the large number of reports, ' +
    'we won’t be able to send a reply.';

/** @type {string} */
const OFFLINE_MESSAGE =
    'Thanks for your feedback. Your feedback helps us improve the Chromebook ' +
    'experience and will be reviewed by our team. Because of the large ' +
    'number of reports, we won’t be able to send a reply.';

export function confirmationPageTest() {
  /** @type {?ConfirmationPageElement} */
  let page = null;

  /** @type {?FakeFeedbackServiceProvider} */
  let feedbackServiceProvider = null;

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
   * @param {boolean} isCalled
   * @param {FeedbackAppPostSubmitAction} action
   * @private
   */
  function verifyRecordPostSubmitActionCalled(isCalled, action) {
    isCalled ?
        assertTrue(
            feedbackServiceProvider.isRecordPostSubmitActionCalled(action)) :
        assertFalse(
            feedbackServiceProvider.isRecordPostSubmitActionCalled(action));
  }

  /**
   * @param {boolean} isOnline
   * @private
   */
  function verifyElementsByStatus(isOnline) {
    if (isOnline) {
      assertEquals(ONLINE_TITLE, getElementContent(page, '.page-title'));
      assertEquals(ONLINE_MESSAGE, getElementContent(page, '#message'));
    } else {
      assertTrue(page.i18nExists('thankYouNoteOnline'));
      assertEquals(OFFLINE_TITLE, getElementContent(page, '.page-title'));
      assertEquals(OFFLINE_MESSAGE, getElementContent(page, '#message'));
      assertTrue(page.i18nExists('confirmationTitleOffline'));
      assertTrue(page.i18nExists('thankYouNoteOffline'));
    }

    // verify help resources exist
    const helpResourcesSection = getElement(page, '#helpResources');
    assertEquals(
        'Here are some other helpful resources:',
        getElementContent(page, '#helpResourcesLabel'));
    assertTrue(page.i18nExists('helpResourcesLabel'));
    const helpLinks = helpResourcesSection.querySelectorAll('cr-link-row');
    assertTrue(!!helpLinks);
    assertEquals(3, helpLinks.length);

    // Verify the explore app link.
    const exploreLink = helpLinks[0];
    assertTrue(isVisible(exploreLink));
    assertEquals(
        'help-resources:explore', getElement(exploreLink, '#startIcon').icon);
    assertEquals('Explore app', getElementContent(page, '#explore > .label'));
    assertTrue(page.i18nExists('exploreAppLabel'));
    assertEquals(
        'Find help articles and answers to common Chromebook questions',
        getElementContent(page, '#explore > .sub-label'));
    assertTrue(page.i18nExists('exploreAppDescription'));

    // Verify the diagnostics app link.
    const diagnosticsLink = helpLinks[1];
    assertTrue(isVisible(diagnosticsLink));
    assertEquals(
        'help-resources:diagnostics',
        getElement(diagnosticsLink, '#startIcon').icon);
    assertEquals(
        'Diagnostics app', getElementContent(page, '#diagnostics > .label'));
    assertTrue(page.i18nExists('diagnosticsAppLabel'));
    assertEquals(
        'Run tests and troubleshooting for hardware issues',
        getElementContent(page, '#diagnostics > .sub-label'));
    assertTrue(page.i18nExists('diagnosticsAppDescription'));

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
        'Chromebook community',
        getElementContent(page, '#chromebookCommunity > .label'));
    assertEquals(
        'Ask the experts in the Chromebook help forum',
        getElementContent(page, '#chromebookCommunity > .sub-label'));
    assertTrue(page.i18nExists('askCommunityLabel'));
    assertTrue(page.i18nExists('askCommunityDescription'));

    // Verify buttons.
    assertEquals(
        'Send new report', getElementContent(page, '#buttonNewReport'));
    assertTrue(page.i18nExists('buttonNewReport'));
    assertEquals('Done', getElementContent(page, '#buttonDone'));
    assertTrue(page.i18nExists('buttonDone'));
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

  /**
   * Test that when send-new-report button is clicked, an on-go-back-click
   * is fired.
   */
  test('SendNewReport', async () => {
    await initializePage();
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kSendNewReport);

    const clickPromise =
        eventToPromise('go-back-click', /**@type {!Element} */ (page));
    let actualCurrentState;

    page.addEventListener('go-back-click', (event) => {
      actualCurrentState = event.detail.currentState;
    });

    const buttonNewReport = getElement(page, '#buttonNewReport');
    buttonNewReport.click();

    await clickPromise;
    assertTrue(!!actualCurrentState);
    assertEquals(FeedbackFlowState.CONFIRMATION, actualCurrentState);
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kSendNewReport);
  });

  // Test clicking done button should close the window.
  test('ClickDoneButtonShouldCloseWindow', async () => {
    await initializePage();
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kClickDoneButton);

    const resolver = new PromiseResolver();
    let windowCloseCalled = 0;

    const closeMock = () => {
      windowCloseCalled++;
      return resolver.promise;
    };
    window.close = closeMock;

    const doneButton = getElement(page, '#buttonDone');
    doneButton.click();
    await flushTasks();

    assertEquals(1, windowCloseCalled);
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kClickDoneButton);
  });

  // Test clicking diagnostics app link.
  test('openDiagnosticsApp', async () => {
    await initializePage();
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);

    assertEquals(0, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());

    const link = getElement(page, '#diagnostics');
    link.click();

    assertEquals(1, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);

    // Make sure that the label and the sub-label are clickable too.
    const label = link.querySelector('.label');
    label.click();
    assertEquals(2, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());

    const subLabel = link.querySelector('.sub-label');
    subLabel.click();
    assertEquals(3, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());
  });

  // Test clicking explore app link.
  test('openExploreApp', async () => {
    await initializePage();
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenExploreApp);

    assertEquals(0, feedbackServiceProvider.getOpenExploreAppCallCount());

    const link = getElement(page, '#explore');
    link.click();

    assertEquals(1, feedbackServiceProvider.getOpenExploreAppCallCount());
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenExploreApp);

    // Make sure that the label and the sub-label are clickable too.
    const label = link.querySelector('.label');
    label.click();
    assertEquals(2, feedbackServiceProvider.getOpenExploreAppCallCount());

    const subLabel = link.querySelector('.sub-label');
    subLabel.click();
    assertEquals(3, feedbackServiceProvider.getOpenExploreAppCallCount());
  });

  // Test clicking openChromebookHelp link.
  test('openChromebookHelp', async () => {
    await initializePage();
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenChromebookCommunity);
    const resolver = new PromiseResolver();
    let windowOpenCalled = 0;
    let url = '';
    let target = '';

    const openMock = (urlArg, targetArg) => {
      windowOpenCalled++;
      url = urlArg;
      target = targetArg;
      return resolver.promise;
    };

    window.open = /** @type {!function()} */ (openMock);

    const link = getElement(page, '#chromebookCommunity');
    link.click();

    await flushTasks();

    assertEquals(1, windowOpenCalled);
    assertEquals(target, '_blank');
    assertEquals(
        url, 'https://support.google.com/chromebook/?hl=en#topic=3399709');
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenChromebookCommunity);

    // Make sure that the label and the sub-label are clickable too.
    const label = link.querySelector('.label');
    label.click();
    assertEquals(2, windowOpenCalled);

    const subLabel = link.querySelector('.sub-label');
    subLabel.click();
    assertEquals(3, windowOpenCalled);
  });

  // Test that we only record the user's first action on confirmation page.
  test('recordFirstPostCompleteAction', async () => {
    await initializePage();

    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenExploreApp);
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);

    // Open explore app first then open diagnostics app, should only record
    // the first user action.
    const exploreLink = getElement(page, '#explore');
    exploreLink.click();
    const diagnosticsLink = getElement(page, '#diagnostics');
    diagnosticsLink.click();
    await flushTasks();

    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenExploreApp);
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);
  });
}
