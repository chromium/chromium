// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConfirmationPageElement} from 'chrome://os-feedback/confirmation_page.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {setFeedbackServiceProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {FeedbackAppPostSubmitAction, SendReportStatus} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

const ONLINE_TITLE = 'Thanks for your feedback';
const OFFLINE_TITLE = 'You\'re offline. Feedback will be sent later.';

const ONLINE_MESSAGE =
    'Your feedback helps us improve the Chromebook experience and will be ' +
    'reviewed by our team. Because of the large number of reports, ' +
    'we won’t be able to send a reply.';

const OFFLINE_MESSAGE =
    'Thanks for your feedback. Your feedback helps us improve the Chromebook ' +
    'experience and will be reviewed by our team. Because of the large ' +
    'number of reports, we won’t be able to send a reply.';

suite('confirmationPageTest', () => {
  let page: ConfirmationPageElement|null = null;
  let feedbackServiceProvider: FakeFeedbackServiceProvider|null = null;
  let openWindowProxy: TestOpenWindowProxy;

  suiteSetup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  });

  function initializePage() {
    page = document.createElement('confirmation-page');
    assert(page);
    page.isUserLoggedIn = true;
    document.body.appendChild(page);
    return flushTasks();
  }

  function getElementContent(host: Element|null, selector: string): string {
    const element = host!.shadowRoot!.querySelector(selector);
    return element!.textContent!.trim();
  }

  function verifyRecordPostSubmitActionCalled(
      isCalled: boolean, action: FeedbackAppPostSubmitAction) {
    assert(feedbackServiceProvider);

    isCalled ?
        assertTrue(
            feedbackServiceProvider.isRecordPostSubmitActionCalled(action)) :
        assertFalse(
            feedbackServiceProvider.isRecordPostSubmitActionCalled(action));
  }

  function verifyElementsByStatus(isOnline: boolean) {
    assert(page);

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

    // verify help resources exist.
    const helpResourcesSection =
        strictQuery('#helpResources', page.shadowRoot, HTMLElement);
    assertEquals(
        'Here are some other helpful resources:',
        getElementContent(page, '#helpResourcesLabel'));
    assertTrue(page.i18nExists('helpResourcesLabel'));
    const helpLinks = helpResourcesSection.querySelectorAll('cr-link-row');
    assertTrue(!!helpLinks);
    assertEquals(3, helpLinks.length);

    // Verify the explore app link.
    const exploreLink = helpLinks[0];
    assert(exploreLink);
    assertTrue(isVisible(exploreLink));
    assertEquals('help-resources:explore', exploreLink.startIcon);
    assertEquals('Explore app', getElementContent(page, '#explore > .label'));
    assertTrue(page.i18nExists('exploreAppLabel'));
    assertEquals(
        'Find help articles and answers to common Chromebook questions',
        getElementContent(page, '#explore > .sub-label'));
    assertTrue(page.i18nExists('exploreAppDescription'));

    // Verify the diagnostics app link.
    const diagnosticsLink = helpLinks[1];
    assert(diagnosticsLink);
    assertTrue(isVisible(diagnosticsLink));
    assertEquals('help-resources:diagnostics', diagnosticsLink.startIcon);
    assertEquals(
        'Diagnostics app', getElementContent(page, '#diagnostics > .label'));
    assertTrue(page.i18nExists('diagnosticsAppLabel'));
    assertEquals(
        'Run tests and troubleshooting for hardware issues',
        getElementContent(page, '#diagnostics > .sub-label'));
    assertTrue(page.i18nExists('diagnosticsAppDescription'));

    // Verify the community link.
    const communityLink = helpLinks[2];
    assert(communityLink);
    if (isOnline && page.isUserLoggedIn) {
      assertTrue(isVisible(communityLink));
    } else {
      assertFalse(isVisible(communityLink));
    }
    assertEquals(
        'help-resources2:chromebook-community', communityLink.startIcon);
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
    assert(page);

    page.sendReportStatus = SendReportStatus.kSuccess;
    verifyElementsByStatus(/**isOnline=*/ true);
  });

  // Test when send report status is unknown, the corresponding title and
  // message are being used. The community link should be visible,
  test('offlineModeStatusUnknown', async () => {
    await initializePage();
    assert(page);

    page.sendReportStatus = SendReportStatus.kUnknown;
    verifyElementsByStatus(/**isOnline=*/ true);
  });

  // Test when send report status is offline, the corresponding title and
  // message are being used. The community link should be invisible,
  test('offlineModeStatusDelayed', async () => {
    await initializePage();
    assert(page);

    page.sendReportStatus = SendReportStatus.kDelayed;
    verifyElementsByStatus(/**isOnline=*/ false);
  });

  /**
   * Test that when the user is not logged in, the help resources section should
   * be invisible.
   */
  test('userNotLoggedIn_ShouldHideHelpResourcesSection', async () => {
    await initializePage();
    assert(page);
    page.isUserLoggedIn = false;

    const helpResourcesSection =
        strictQuery('#helpResources', page.shadowRoot, HTMLElement);
    assertFalse(isVisible(helpResourcesSection));
  });

  /**
   * Test that when the user is logged in, the help resources section should be
   * visible.
   */
  test('userLoggedIn_ShouldShowHelpResourcesSection', async () => {
    await initializePage();
    assert(page);
    page.isUserLoggedIn = true;

    const helpResourcesSection =
        strictQuery('#helpResources', page.shadowRoot, HTMLElement);
    assertTrue(isVisible(helpResourcesSection));
  });

  /**
   * Test that when send-new-report button is clicked, an on-go-back-click
   * is fired.
   */
  test('SendNewReport', async () => {
    await initializePage();
    assert(page);

    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kSendNewReport);

    const clickPromise =
        eventToPromise('go-back-click', /**@type {!Element} */ (page));
    let actualCurrentState;

    page.addEventListener('go-back-click', (event) => {
      actualCurrentState = event.detail.currentState;
    });

    const buttonNewReport =
        strictQuery('#buttonNewReport', page.shadowRoot, HTMLElement);
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
    assert(page);

    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kClickDoneButton);

    const resolver = new PromiseResolver();
    let windowCloseCalled = 0;

    const closeMock = () => {
      windowCloseCalled++;
      return resolver.promise;
    };
    window.close = closeMock;

    const doneButton = strictQuery('#buttonDone', page.shadowRoot, HTMLElement);
    doneButton.click();
    await flushTasks();

    assertEquals(1, windowCloseCalled);
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kClickDoneButton);
  });

  // Test clicking diagnostics app link.
  test('openDiagnosticsApp', async () => {
    await initializePage();
    assert(page);
    assert(feedbackServiceProvider);

    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);

    assertEquals(0, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());

    const link = strictQuery('#diagnostics', page.shadowRoot, HTMLElement);
    link.click();

    assertEquals(1, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);

    // Make sure that the label and the sub-label are clickable too.
    const label = link.querySelector<HTMLElement>('.label');
    assert(label);
    label.click();
    assertEquals(2, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());

    const subLabel = link.querySelector<HTMLElement>('.sub-label');
    assert(subLabel);
    subLabel.click();
    assertEquals(3, feedbackServiceProvider.getOpenDiagnosticsAppCallCount());
  });

  // Test clicking explore app link.
  test('openExploreApp', async () => {
    await initializePage();
    assert(page);
    assert(feedbackServiceProvider);

    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenExploreApp);

    assertEquals(0, feedbackServiceProvider.getOpenExploreAppCallCount());

    const link = strictQuery('#explore', page.shadowRoot, HTMLElement);
    link.click();

    assertEquals(1, feedbackServiceProvider.getOpenExploreAppCallCount());
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenExploreApp);

    // Make sure that the label and the sub-label are clickable too.
    const label = link.querySelector<HTMLElement>('.label');
    assert(label);
    label.click();
    assertEquals(2, feedbackServiceProvider.getOpenExploreAppCallCount());

    const subLabel = link.querySelector<HTMLElement>('.sub-label');
    assert(subLabel);
    subLabel.click();
    assertEquals(3, feedbackServiceProvider.getOpenExploreAppCallCount());
  });

  // Test clicking openChromebookHelp link.
  test('openChromebookHelp', async () => {
    await initializePage();
    assert(page);
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenChromebookCommunity);

    const expectedUrl =
        'https://support.google.com/chromebook/?hl=en#topic=3399709';
    let url = '';

    const link =
        strictQuery('#chromebookCommunity', page.shadowRoot, HTMLElement);
    link.click();

    url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, expectedUrl);
    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenChromebookCommunity);

    // Make sure that the label and the sub-label are clickable too.
    const label = link.querySelector<HTMLElement>('.label');
    assert(label);
    openWindowProxy.resetResolver('openUrl');

    label.click();
    url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, expectedUrl);

    const subLabel = link.querySelector<HTMLElement>('.sub-label');
    assert(subLabel);
    openWindowProxy.resetResolver('openUrl');

    subLabel.click();
    url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, expectedUrl);
  });

  // Test that we only record the user's first action on confirmation page.
  test('recordFirstPostCompleteAction', async () => {
    await initializePage();
    assert(page);

    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenExploreApp);
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);

    // Open explore app first then open diagnostics app, should only record
    // the first user action.
    const exploreLink = strictQuery('#explore', page.shadowRoot, HTMLElement);
    exploreLink.click();
    const diagnosticsLink =
        strictQuery('#diagnostics', page.shadowRoot, HTMLElement);
    diagnosticsLink.click();
    await flushTasks();

    verifyRecordPostSubmitActionCalled(
        true, FeedbackAppPostSubmitAction.kOpenExploreApp);
    verifyRecordPostSubmitActionCalled(
        false, FeedbackAppPostSubmitAction.kOpenDiagnosticsApp);
  });
});
