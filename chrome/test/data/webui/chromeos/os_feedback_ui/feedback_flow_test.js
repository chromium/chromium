// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeFeedbackContext, fakeInternalUserFeedbackContext, fakePngData, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {AdditionalContextQueryParam, FeedbackFlowElement, FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {FeedbackAppExitPath, FeedbackAppHelpContentOutcome, FeedbackAppPreSubmitAction, FeedbackContext, SendReportStatus} from 'chrome://os-feedback/feedback_types.js';
import {OS_FEEDBACK_TRUSTED_ORIGIN} from 'chrome://os-feedback/help_content.js';
import {setFeedbackServiceProviderForTesting, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {SearchPageElement} from 'chrome://os-feedback/search_page.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise, isVisible} from '../test_util.js';

export function FeedbackFlowTestSuite() {
  /** @type {?FeedbackFlowElement} */
  let page = null;

  /** @type {?FakeHelpContentProvider} */
  let helpContentProvider = null;

  /** @type {?FakeFeedbackServiceProvider} */
  let feedbackServiceProvider = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
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

  /**
   * @param {boolean} isCalled
   * @param {FeedbackAppExitPath} exitPath
   * @private
   */
  function verifyRecordExitPathCalled(isCalled, exitPath) {
    isCalled ?
        assertTrue(feedbackServiceProvider.isRecordExitPathCalled(exitPath)) :
        assertFalse(feedbackServiceProvider.isRecordExitPathCalled(exitPath));
  }


  /**
   * @param {boolean} isCalled
   * @param {FeedbackAppHelpContentOutcome} outcome
   * @private
   */
  function verifyHelpContentOutcomeMetricCalled(isCalled, outcome) {
    isCalled ?
        assertTrue(feedbackServiceProvider.isHelpContentOutcomeMetricEmitted(
            outcome)) :
        assertFalse(
            feedbackServiceProvider.isHelpContentOutcomeMetricEmitted(outcome));
  }

  /**
   * @param {FeedbackFlowState} exitPage
   * @param {FeedbackAppExitPath} exitPath
   * @param {boolean} helpContentClicked
   * @private
   */
  function verifyExitPathMetricsEmitted(
      exitPage, exitPath, helpContentClicked) {
    page.setCurrentStateForTesting(exitPage);
    page.setHelpContentClickedForTesting(helpContentClicked);

    verifyRecordExitPathCalled(/*metric_emitted=*/ false, exitPath);
    window.dispatchEvent(new CustomEvent('beforeunload'));
    verifyRecordExitPathCalled(/*metric_emitted=*/ true, exitPath);
  }

  /**
   * @private
   */
  function testWithInternalAccount() {
    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    feedbackServiceProvider.setFakeFeedbackContext(
        fakeInternalUserFeedbackContext);
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  }

  /**
   * @param {boolean} from_assistant
   * @private
   */
  function setFromAssistantFlag(from_assistant) {
    if (from_assistant) {
      const queryParams = new URLSearchParams(window.location.search);
      const from_assistant = 'true';
      queryParams.set(
          AdditionalContextQueryParam.FROM_ASSISTANT, from_assistant);

      window.history.replaceState(null, '', '?' + queryParams.toString());
    } else {
      window.history.replaceState(
          null, '',
          '?' +
              '');
    }
  }

  /**
   * @param {boolean} fromSettingsSearch
   * @private
   */
  function setFromSettingsSearchFlag(fromSettingsSearch) {
    if (fromSettingsSearch) {
      const queryParams = new URLSearchParams(window.location.search);
      const fromSettingsSearch = 'true';
      queryParams.set(
          AdditionalContextQueryParam.FROM_SETTINGS_SEARCH, fromSettingsSearch);

      window.history.replaceState(null, '', '?' + queryParams.toString());
    } else {
      window.history.replaceState(
          null, '',
          '?' +
              '');
    }
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

    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kContinueHelpContentClicked);
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kContinueNoHelpContentClicked);

    page.setHelpContentClickedForTesting(true);

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
    // Verify that click continue after viewing helpcontent will emit the
    // correct metric.
    verifyHelpContentOutcomeMetricCalled(
        true, FeedbackAppHelpContentOutcome.kContinueHelpContentClicked);
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kContinueNoHelpContentClicked);
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

  // Test the bluetooth logs will show up if logged with internal account and
  // input description is related.
  test('ShowBluetoothLogsWithRelatedDescription', async () => {
    testWithInternalAccount();
    await initializePage();

    // Check the bluetooth checkbox component hidden when input is not related
    // to bluetooth.
    let activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    activePage.shadowRoot.querySelector('textarea').value = 'abc';
    activePage.shadowRoot.querySelector('#buttonContinue').click();
    await flushTasks();

    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);
    const bluetoothCheckbox =
        activePage.shadowRoot.querySelector('#bluetoothCheckboxContainer');
    assertTrue(!!bluetoothCheckbox);
    assertFalse(isVisible(bluetoothCheckbox));

    activePage.shadowRoot.querySelector('#buttonBack').click();
    await flushTasks();

    // Go back to search page and set description input related to bluetooth.
    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    const descriptionElement = activePage.shadowRoot.querySelector('textarea');
    descriptionElement.value = 'bluetooth';

    activePage.shadowRoot.querySelector('#buttonContinue').click();
    await flushTasks();

    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('shareDataPage', activePage.id);

    assertTrue(!!bluetoothCheckbox);
    assertTrue(isVisible(bluetoothCheckbox));
  });

  // Test the bluetooth logs will not show up if not logged with an Internal
  // google account.
  test('BluetoothHiddenWithoutInternalAccount', async () => {
    await initializePage();

    // Set input description related to bluetooth.
    let activePage = page.shadowRoot.querySelector('.iron-selected');

    activePage.shadowRoot.querySelector('textarea').value = 'bluetooth';
    activePage.shadowRoot.querySelector('#buttonContinue').click();
    await flushTasks();

    activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);
    const bluetoothCheckbox =
        activePage.shadowRoot.querySelector('#bluetoothCheckboxContainer');
    assertTrue(!!bluetoothCheckbox);
    assertFalse(isVisible(bluetoothCheckbox));
  });

  // Test the assistant logs will show up if logged with internal account and
  // the fromAssistant flag is true.
  test('ShowAssistantCheckboxWithInternalAccountAndFlagSetTrue', async () => {
    // Replacing the query string to set the fromAssistant flag as true.
    setFromAssistantFlag(true);
    testWithInternalAccount();
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    const feedbackContext = getFeedbackContext_();
    assertTrue(feedbackContext.isInternalAccount);
    assertTrue(feedbackContext.fromAssistant);
    // Check the assistant checkbox component visible when input is not
    // related to bluetooth.
    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertEquals('shareDataPage', activePage.id);

    const assistantCheckbox =
        activePage.shadowRoot.querySelector('#assistantLogsContainer');

    assertTrue(!!assistantCheckbox);
    assertTrue(isVisible(assistantCheckbox));
  });

  // Test the assistant checkbox will not show up to external account user
  // with fromAssistant flag passed.
  test('AssistantCheckboxHiddenWithExternalAccount', async () => {
    // Replacing the query string to set the fromAssistant flag as true.
    setFromAssistantFlag(true);
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    const feedbackContext = getFeedbackContext_();
    assertFalse(feedbackContext.isInternalAccount);
    assertTrue(feedbackContext.fromAssistant);
    let activePage = page.shadowRoot.querySelector('.iron-selected');
    activePage = page.shadowRoot.querySelector('.iron-selected');

    assertEquals('shareDataPage', activePage.id);
    const assistantCheckbox =
        activePage.shadowRoot.querySelector('#assistantLogsContainer');
    assertTrue(!!assistantCheckbox);
    assertFalse(isVisible(assistantCheckbox));
  });

  // Test the assistant logs will not show up if fromAssistant flag is not
  // passed but logged in with Internal google account.
  test('AssistantCheckboxHiddenWithoutFlagPassed', async () => {
    // Replace the current querystring back to default.
    setFromAssistantFlag(false);
    // Set Internal Account flag as true.
    testWithInternalAccount();
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    const feedbackContext = getFeedbackContext_();
    assertTrue(feedbackContext.isInternalAccount);
    assertFalse(feedbackContext.fromAssistant);
    // Set input description related to bluetooth.
    let activePage = page.shadowRoot.querySelector('.iron-selected');
    activePage = page.shadowRoot.querySelector('.iron-selected');

    assertEquals('shareDataPage', activePage.id);
    const assistantCheckbox =
        activePage.shadowRoot.querySelector('#assistantLogsContainer');
    assertTrue(!!assistantCheckbox);
    assertFalse(isVisible(assistantCheckbox));
    // Set the flag back to true.
    fakeInternalUserFeedbackContext.fromAssistant = true;
  });

  // Test the sys info and metrics checkbox will not be checked if
  // fromSettingsSearch flag has been passed.
  test(
      'SysinfoAndMetricsCheckboxIsUncheckedWhenFeedbackIsSentFromSettingsSearch',
      async () => {
        // Replacing the query string to set the fromSettingsSearch flag as
        // true.
        setFromSettingsSearchFlag(true);
        await initializePage();
        const feedbackContext = getFeedbackContext_();
        assertTrue(feedbackContext.fromSettingsSearch);

        let activePage = page.shadowRoot.querySelector('.iron-selected');
        activePage.shadowRoot.querySelector('textarea').value = 'text';
        activePage.shadowRoot.querySelector('#buttonContinue').click();
        await flushTasks();

        // Check the sys info and metrics checkbox component is unchecked when
        // the feedback app has opened through settings search
        activePage = page.shadowRoot.querySelector('.iron-selected');
        assertEquals('shareDataPage', activePage.id);

        const sysInfoAndMetricsCheckboxContainer =
            activePage.shadowRoot.querySelector('#sysInfoContainer');
        assertTrue(!!sysInfoAndMetricsCheckboxContainer);

        const sysInfoAndMetricsCheckbox =
            activePage.shadowRoot.querySelector('#sysInfoCheckbox');
        assertTrue(!!sysInfoAndMetricsCheckbox);
        assertFalse(sysInfoAndMetricsCheckbox.checked);
      });

  // Test the sys info and metrics checkbox will be checked if
  // fromSettingsSearch flag not passed.
  test(
      'SysinfoAndMetricsCheckboxIsCheckedWhenFeedbackIsNotSentFromSettingsSearch',
      async () => {
        // Replacing the query string to set the fromSettingsSearch flag as
        // false.
        setFromSettingsSearchFlag(false);
        await initializePage();
        const feedbackContext = getFeedbackContext_();
        assertFalse(feedbackContext.fromSettingsSearch);

        let activePage = page.shadowRoot.querySelector('.iron-selected');
        activePage.shadowRoot.querySelector('textarea').value = 'text';
        activePage.shadowRoot.querySelector('#buttonContinue').click();
        await flushTasks();

        activePage = page.shadowRoot.querySelector('.iron-selected');
        assertEquals('shareDataPage', activePage.id);

        const sysInfoAndMetricsContainer =
            activePage.shadowRoot.querySelector('#sysInfoContainer');
        assertTrue(!!sysInfoAndMetricsContainer);

        const sysInfoAndMetricsCheckbox =
            activePage.shadowRoot.querySelector('#sysInfoCheckbox');
        assertTrue(!!sysInfoAndMetricsCheckbox);
        assertTrue(sysInfoAndMetricsCheckbox.checked);
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

  // Test that the extra diagnostics, category tag, page_url, fromAssistant
  // and fromSettingsSearch flag get set when query parameter is non-empty.
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
        const description_placeholder_text =
            'Thanks%20for%20giving%20feedback%20on%20the%20Camera%20app';
        queryParams.set(
            AdditionalContextQueryParam.DESCRIPTION_PLACEHOLDER_TEXT,
            description_placeholder_text);
        const category_tag = 'some%20category%20tag';
        queryParams.set(AdditionalContextQueryParam.CATEGORY_TAG, category_tag);
        const page_url = 'some%20page%20url';
        queryParams.set(AdditionalContextQueryParam.PAGE_URL, page_url);
        const from_assistant = 'true';
        queryParams.set(
            AdditionalContextQueryParam.FROM_ASSISTANT, from_assistant);
        const fromSettingsSearch = 'true';
        queryParams.set(
            AdditionalContextQueryParam.FROM_SETTINGS_SEARCH,
            fromSettingsSearch);
        // Replace current querystring with the new one.
        window.history.replaceState(null, '', '?' + queryParams.toString());
        await initializePage();
        page.setCurrentStateForTesting(FeedbackFlowState.SEARCH);
        const descriptionElement = getSearchPage().$['descriptionText'];

        const feedbackContext = getFeedbackContext_();
        assertEquals(page_url, feedbackContext.pageUrl.url);
        assertEquals(fakeFeedbackContext.email, feedbackContext.email);
        assertEquals(
            decodeURIComponent(extra_diagnostics),
            feedbackContext.extraDiagnostics);
        assertEquals(
            decodeURIComponent(description_template), descriptionElement.value);
        assertEquals(
            decodeURIComponent(description_placeholder_text),
            descriptionElement.placeholder);
        assertEquals(
            decodeURIComponent(category_tag), feedbackContext.categoryTag);
        assertTrue(feedbackContext.fromAssistant);
        assertTrue(feedbackContext.fromSettingsSearch);

        // Set the pageUrl in fake feedback context back to its origin value
        // because it's overwritten by the page_url passed from the app.
        fakeFeedbackContext.pageUrl = {url: 'chrome://tab/'};
      });

  // Test that the extra diagnostics gets set, and pageUrl uses the one passed
  // from the feedbackContext when query parameter is empty.
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
        assertEquals('', feedbackContext.categoryTag);
        assertFalse(feedbackContext.fromAssistant);
        assertFalse(feedbackContext.fromSettingsSearch);
      });

  /**
   * Test that the untrusted page can send "help-content-clicked" message to
   * feedback flow page via postMessage.
   */
  test('CanCommunicateWithUntrustedPage', async () => {
    // Whether feedback flow page has received that help content has been
    // clicked;
    let helpContentClicked = false;
    await initializePage();

    assertEquals(
        0,
        feedbackServiceProvider.getRecordPreSubmitActionCallCount(
            FeedbackAppPreSubmitAction.kViewedHelpContent));
    assertEquals(
        0, feedbackServiceProvider.getRecordHelpContentSearchResultCount());

    // Get Search Page.
    const SearchPage = getSearchPage();
    const iframe = /** @type {!HTMLIFrameElement} */ (
        SearchPage.shadowRoot.querySelector('iframe'));

    assertTrue(!!iframe);
    // Wait for the iframe completes loading.
    await eventToPromise('load', iframe);

    // There is another message posted from iframe which sends the height of
    // the help content.
    const expectedMessageEventCount = 2;
    let messageEventCount = 0;
    const resolver = new PromiseResolver();

    window.addEventListener('message', event => {
      if ('help-content-clicked-for-testing' === event.data.id &&
          OS_FEEDBACK_TRUSTED_ORIGIN === event.origin) {
        helpContentClicked = true;
        feedbackServiceProvider.recordPreSubmitAction(
            FeedbackAppPreSubmitAction.kViewedHelpContent);
        feedbackServiceProvider.recordHelpContentSearchResultCount();
      }
      messageEventCount++;
      if (messageEventCount === expectedMessageEventCount) {
        resolver.resolve();
      }
    });

    // Data to be posted from untrusted page to feedback flow page.
    const data = {
      id: 'help-content-clicked-for-testing',
    };
    iframe.contentWindow.parent.postMessage(data, OS_FEEDBACK_TRUSTED_ORIGIN);

    // Wait for the "help-content-clicked" message has been received.
    await resolver.promise;

    // Verify that help content have been clicked.
    assertTrue(helpContentClicked);
    // Verify that viewedHelpContent metrics is emitted.
    assertEquals(
        1,
        feedbackServiceProvider.getRecordPreSubmitActionCallCount(
            FeedbackAppPreSubmitAction.kViewedHelpContent));
    // Verify that clicks the help content will emit the
    // recordHelpContentSearchResult metric.
    assertEquals(
        1, feedbackServiceProvider.getRecordHelpContentSearchResultCount());
  });

  // Test that correct exitPathMetrics is emitted when user clicks help content
  // and quits on search page.
  test('QuitSearchPageHelpContentClicked', async () => {
    await initializePage();
    verifyExitPathMetricsEmitted(
        FeedbackFlowState.SEARCH,
        FeedbackAppExitPath.kQuitSearchPageHelpContentClicked, true);
  });

  // Test that correct exitPathMetrics is emitted when user quits on search page
  // without clicking any help contents.
  test('QuitSearchPageNoHelpContentClicked', async () => {
    await initializePage();
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kQuitHelpContentClicked);
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kQuitNoHelpContentClicked);

    // This will emit an close-app event with no help content clicked.
    verifyExitPathMetricsEmitted(
        FeedbackFlowState.SEARCH,
        FeedbackAppExitPath.kQuitSearchPageNoHelpContentClicked, false);

    // Verify that close app without viewing helpcontent will emit the
    // correct metric.
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kQuitHelpContentClicked);
    verifyHelpContentOutcomeMetricCalled(
        true, FeedbackAppHelpContentOutcome.kQuitNoHelpContentClicked);
  });

  // Test that correct metrics are emitted when user quits on
  // search page because no help content is shown.
  test('QuitSearchPagesetNoHelpContentDisplayed', async () => {
    await initializePage();
    page.setNoHelpContentDisplayedForTesting(true);

    verifyRecordExitPathCalled(
        false, FeedbackAppExitPath.kQuitNoHelpContentDisplayed);
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kQuitNoHelpContentDisplayed);

    window.dispatchEvent(new CustomEvent('beforeunload'));

    verifyRecordExitPathCalled(
        true, FeedbackAppExitPath.kQuitNoHelpContentDisplayed);
    verifyHelpContentOutcomeMetricCalled(
        true, FeedbackAppHelpContentOutcome.kQuitNoHelpContentDisplayed);
  });

  // Test that correct exitPathMetrics is emitted when user quits on share data
  // page.
  test('QuitShareDataPageHelpContentClicked', async () => {
    await initializePage();
    verifyExitPathMetricsEmitted(
        FeedbackFlowState.SHARE_DATA,
        FeedbackAppExitPath.kQuitShareDataPageHelpContentClicked, true);
  });

  // Test that correct exitPathMetrics is emitted when user quits on share data
  // page.
  test('QuitShareDataPageNoHelpContentClicked', async () => {
    await initializePage();
    verifyExitPathMetricsEmitted(
        FeedbackFlowState.SHARE_DATA,
        FeedbackAppExitPath.kQuitShareDataPageNoHelpContentClicked, false);
  });

  // Test that correct exitPathMetrics is emitted when user clicks help content
  // and quits on confirmation page.
  test('QuitConfirmationPageHelpContentClicked', async () => {
    await initializePage();
    verifyExitPathMetricsEmitted(
        FeedbackFlowState.CONFIRMATION,
        FeedbackAppExitPath.kSuccessHelpContentClicked, true);
  });

  // Test that correct exitPathMetrics is emitted when user quits on
  // confirmation page without clicking any help contents.
  test('QuitConfirmationPageNoHelpContentClicked', async () => {
    await initializePage();
    verifyExitPathMetricsEmitted(
        FeedbackFlowState.CONFIRMATION,
        FeedbackAppExitPath.kSuccessNoHelpContentClicked, false);
  });

  // Test that correct helpContentOutcomeMetrics is emitted when user click
  // continue when there is no help content shown.
  test('HelpContentOutcomeContinuesetNoHelpContentDisplayed', async () => {
    await initializePage();
    page.setNoHelpContentDisplayedForTesting(true);
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kContinueNoHelpContentDisplayed);

    // Now on search page.
    const activePage = page.shadowRoot.querySelector('.iron-selected');
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);
    const inputElement = activePage.shadowRoot.querySelector('textarea');
    const continueButton =
        activePage.shadowRoot.querySelector('#buttonContinue');

    // Enter some text.
    inputElement.value = 'abc';
    continueButton.click();

    verifyHelpContentOutcomeMetricCalled(
        true, FeedbackAppHelpContentOutcome.kContinueNoHelpContentDisplayed);
  });

  test(
      'UpdatesCSSUrlBasedOnIsJellyEnabledForOsFeedback_TrustedUi', async () => {
        // Setup test for jelly disabled.
        loadTimeData.overrideValues({
          isJellyEnabledForOsFeedback: false,
        });
        /*@type {HTMLLinkElement}*/
        const link = document.createElement('link');
        const disabledUrl =
            'chrome://resources/chromeos/colors/cros_styles.css';
        link.href = disabledUrl;
        document.head.appendChild(link);
        await initializePage();

        assertTrue(link.href.includes(disabledUrl));

        // Reset app element.
        document.body.innerHTML = trustedTypes.emptyHTML;
        page.remove();
        page = null;

        // Setup test for jelly enabled.
        loadTimeData.overrideValues({
          isJellyEnabledForOsFeedback: true,
        });
        await initializePage();

        const enabledUrl = 'theme/colors.css';
        assertTrue(link.href.includes(enabledUrl));

        // Clean up test specific element.
        document.head.removeChild(link);
      });

  // Test that test helper message is triggered on untrusted UI when
  // `isJellyEnabledForOsFeedback` is true.
  test(
      'UpdatesCSSUrlBasedOnIsJellyEnabledForOsFeedback_UntrustedUi',
      async () => {
        // `isJellyEnabledForOsFeedback` is true by default based on test flag
        // configuration.
        assertTrue(loadTimeData.getBoolean('isJellyEnabledForOsFeedback'));

        const resolver = new PromiseResolver();
        let colorChangeUpdaterCalled = false;
        const testMessageListener = (event) => {
          if (event.data.id === 'color-change-updater-started-for-testing') {
            colorChangeUpdaterCalled = true;
            resolver.resolve();
          }
        };
        window.addEventListener('message', testMessageListener);

        await initializePage();
        await resolver.promise;

        assertTrue(colorChangeUpdaterCalled);

        window.removeEventListener('message', testMessageListener);
      });
}
