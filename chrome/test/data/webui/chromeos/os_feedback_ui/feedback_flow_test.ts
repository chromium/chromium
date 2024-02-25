// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-feedback/confirmation_page.js';
import 'chrome://os-feedback/search_page.js';
import 'chrome://os-feedback/share_data_page.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConfirmationPageElement} from 'chrome://os-feedback/confirmation_page.js';
import {fakeFeedbackContext, fakeFeedbackContextWithoutLinkedCrossDevicePhone, fakeInternalUserFeedbackContext, fakePngData, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {AdditionalContextQueryParam, FeedbackFlowButtonClickEvent, FeedbackFlowElement, FeedbackFlowState} from 'chrome://os-feedback/feedback_flow.js';
import {OS_FEEDBACK_TRUSTED_ORIGIN} from 'chrome://os-feedback/help_content.js';
import {setFeedbackServiceProviderForTesting, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {FeedbackAppExitPath, FeedbackAppHelpContentOutcome, FeedbackAppPreSubmitAction, FeedbackContext, SendReportStatus} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {SearchPageElement} from 'chrome://os-feedback/search_page.js';
import {ShareDataPageElement} from 'chrome://os-feedback/share_data_page.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('FeedbackFlowTestSuite', () => {
  let page: FeedbackFlowElement;

  let helpContentProvider: FakeHelpContentProvider;

  let feedbackServiceProvider: FakeFeedbackServiceProvider;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

  function initializePage() {
    page = document.createElement('feedback-flow');
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  function findChildElement(element: Element, selector: string): Element|null {
    return element.shadowRoot!.querySelector(selector);
  }

  function getFeedbackContext(): FeedbackContext {
    return page!.getFeedbackContextForTesting() as FeedbackContext;
  }

  function getSearchPage(): SearchPageElement {
    return strictQuery('#searchPage', page!.shadowRoot, SearchPageElement);
  }

  function getDescriptionElement(): HTMLTextAreaElement {
    return strictQuery(
        '#descriptionText', getSearchPage().shadowRoot, HTMLTextAreaElement);
  }

  type ActivePageElement =
      SearchPageElement|ShareDataPageElement|ConfirmationPageElement;

  function getActivePage<T extends ActivePageElement>(): T {
    return page!.shadowRoot!.querySelector('.iron-selected') as T;
  }

  function verifyRecordExitPathCalled(
      isCalled: boolean, exitPath: FeedbackAppExitPath) {
    isCalled ?
        assertTrue(feedbackServiceProvider.isRecordExitPathCalled(exitPath)) :
        assertFalse(feedbackServiceProvider.isRecordExitPathCalled(exitPath));
  }

  function verifyHelpContentOutcomeMetricCalled(
      isCalled: boolean, outcome: FeedbackAppHelpContentOutcome) {
    isCalled ?
        assertTrue(feedbackServiceProvider.isHelpContentOutcomeMetricEmitted(
            outcome)) :
        assertFalse(
            feedbackServiceProvider.isHelpContentOutcomeMetricEmitted(outcome));
  }

  function verifyExitPathMetricsEmitted(
      exitPage: FeedbackFlowState, exitPath: FeedbackAppExitPath,
      helpContentClicked: boolean) {
    page.setCurrentStateForTesting(exitPage);
    page.setHelpContentClickedForTesting(helpContentClicked);

    verifyRecordExitPathCalled(/*metric_emitted=*/ false, exitPath);
    window.dispatchEvent(new CustomEvent('beforeunload'));
    verifyRecordExitPathCalled(/*metric_emitted=*/ true, exitPath);
  }

  function testWithInternalAccount() {
    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    feedbackServiceProvider.setFakeFeedbackContext(
        fakeInternalUserFeedbackContext);
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  }

  function setupTestWithoutLinkedCrossDevicePhone() {
    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    feedbackServiceProvider.setFakeFeedbackContext(
        fakeFeedbackContextWithoutLinkedCrossDevicePhone);
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  }

  function setFromAssistantFlag(fromAssistant: boolean) {
    if (fromAssistant) {
      const queryParams = new URLSearchParams(window.location.search);
      const fromAssistant = 'true';
      queryParams.set(
          AdditionalContextQueryParam.FROM_ASSISTANT, fromAssistant);

      window.history.replaceState(null, '', '?' + queryParams.toString());
    } else {
      window.history.replaceState(
          null, '',
          '?' +
              '');
    }
  }

  function setFromSettingsSearchFlag(fromSettingsSearch: boolean) {
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
    const activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // Verify the title is in the page.
    const title =
        strictQuery('.page-title', activePage.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Send feedback', title.textContent!.trim());

    // Verify the continue button is in the page.
    const buttonContinue =
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement);
    assertTrue(!!buttonContinue);
    assertEquals('Continue', buttonContinue.textContent!.trim());
  });


  // Test that the share data page is shown.
  test('ShareDataPageIsShown', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    const activePage = getActivePage<ShareDataPageElement>();
    assertEquals('shareDataPage', activePage.id);

    assertTrue(!!activePage);
    // Verify the title is in the page.
    const title =
        strictQuery('.page-title', activePage.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Send feedback', title.textContent!.trim());

    // Verify the back button is in the page.
    const buttonBack =
        strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement);
    assertTrue(!!buttonBack);
    assertEquals('Back', buttonBack.textContent!.trim());

    // Verify the send button is in the page.
    const buttonSend =
        strictQuery('#buttonSend', activePage.shadowRoot, CrButtonElement);
    assertTrue(!!buttonSend);
    assertEquals('Send', buttonSend.textContent!.trim());
  });


  // Test that the confirmation page is shown.
  test('ConfirmationPageIsShown', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.CONFIRMATION);
    page.setSendReportStatusForTesting(SendReportStatus.kSuccess);

    const activePage = getActivePage<ConfirmationPageElement>();
    assertTrue(!!activePage);
    assertEquals('confirmationPage', activePage.id);

    // Verify the title is in the page.
    const title =
        strictQuery('.page-title', activePage.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Thanks for your feedback', title.textContent!.trim());

    // Verify the done button is in the page.
    const buttonDone =
        strictQuery('#buttonDone', activePage.shadowRoot, CrButtonElement);
    assertTrue(!!buttonDone);
    assertEquals('Done', buttonDone.textContent!.trim());

    // Verify the startNewReport button is in the page.
    const buttonNewReport = strictQuery(
        '#buttonNewReport', activePage.shadowRoot, CrButtonElement);
    assertTrue(!!buttonNewReport);
    assertEquals('Send new report', buttonNewReport.textContent!.trim());
  });

  // Test the navigation from search page to share data page.
  test('NavigateFromSearchPageToShareDataPage', async () => {
    await initializePage();

    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kContinueHelpContentClicked);
    verifyHelpContentOutcomeMetricCalled(
        false, FeedbackAppHelpContentOutcome.kContinueNoHelpContentClicked);

    page.setHelpContentClickedForTesting(true);

    let activePage: ActivePageElement = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    const inputElement =
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement);
    const continueButton =
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement);

    // Clear the description.
    inputElement.value = '';
    continueButton.click();
    await flushTasks();
    // Should stay on search page when click the continue button.
    activePage = getActivePage();
    assertEquals('searchPage', activePage.id);
    assertEquals(0, feedbackServiceProvider.getScreenshotPngCallCount());
    feedbackServiceProvider.setFakeScreenshotPng(fakePngData);

    const clickPromise = eventToPromise('continue-click', page);

    let eventDetail: FeedbackFlowButtonClickEvent;
    page.addEventListener(
        'continue-click', (event: FeedbackFlowButtonClickEvent) => {
          eventDetail = event;
        });

    // Enter some text.
    inputElement.value = 'abc';
    continueButton.click();
    await clickPromise;

    assertEquals(FeedbackFlowState.SEARCH, eventDetail!.detail!.currentState);
    assertEquals('abc', eventDetail!.detail!.description);

    // Should move to share data page when click the continue button.
    activePage = getActivePage<ShareDataPageElement>();
    assertEquals('shareDataPage', activePage.id);

    // Verify that the getScreenshotPng is called once.
    assertEquals(1, feedbackServiceProvider.getScreenshotPngCallCount());
    const screenshotImg = strictQuery(
        '#screenshotImage', activePage.shadowRoot, HTMLImageElement);
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

    let activePage = getActivePage();
    assertEquals('shareDataPage', activePage.id);

    strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement).click();
    await flushTasks();
    // Should go back to share data page.
    activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    // The description input element should have received focused.
    const descriptionElement =
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement);
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

    let activePage = getActivePage();
    assertEquals('shareDataPage', activePage.id);

    const clickPromise = eventToPromise('continue-click', page);

    let eventDetail;
    page.addEventListener('continue-click', (event) => {
      eventDetail = event;
    });

    assertEquals(0, feedbackServiceProvider.getSendReportCallCount());

    strictQuery('#buttonSend', activePage.shadowRoot, CrButtonElement).click();
    await clickPromise;

    // Verify the sendReport method was invoked.
    assertEquals(1, feedbackServiceProvider.getSendReportCallCount());
    assertEquals(
        FeedbackFlowState.SHARE_DATA, eventDetail!.detail!.currentState);

    // Should navigate to confirmation page.
    activePage = getActivePage<ConfirmationPageElement>();
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
    let activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement).value =
        'abc';
    strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    activePage = getActivePage();
    assertEquals('shareDataPage', activePage.id);
    const bluetoothCheckbox = strictQuery(
        '#bluetoothCheckboxContainer', activePage.shadowRoot, HTMLElement);
    assertTrue(!!bluetoothCheckbox);
    assertFalse(isVisible(bluetoothCheckbox));

    strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement).click();
    await flushTasks();

    // Go back to search page and set description input related to bluetooth.
    activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    const descriptionElement =
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement);
    descriptionElement.value = 'bluetooth';

    strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    activePage = getActivePage();
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
    let activePage = getActivePage();

    strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement).value =
        'bluetooth';
    strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    activePage = getActivePage();
    assertEquals('shareDataPage', activePage.id);
    const bluetoothCheckbox = strictQuery(
        '#bluetoothCheckboxContainer', activePage.shadowRoot, HTMLElement);
    assertTrue(!!bluetoothCheckbox);
    assertFalse(isVisible(bluetoothCheckbox));
  });

  // Test that the flag ShouldShowWifiDebugLogsCheckBox_ is false if
  // - is not internal account.
  // - wifi, wi-fi, internet, network, and hotspot are mentioned in description.
  test('DoNotShowWifiDebugLogsCheckBox_ExternalAccount', async () => {
    await initializePage();
    assertFalse(page.getShouldShowWifiDebugLogsCheckboxForTesting());

    const searchPage = findChildElement(page, '.iron-selected');
    assertTrue(!!searchPage);
    assertEquals('searchPage', searchPage!.id);

    const inputElement = findChildElement(searchPage as Element, 'textarea') as
        HTMLTextAreaElement;
    inputElement.value = 'wifi wi-fi internet network hotspot';
    // The flag ShouldShowWifiDebugLogsCheckBox_ is only updated when continue
    // button is clicked.
    const continueButton =
        findChildElement(searchPage as Element, '#buttonContinue') as
        CrButtonElement;
    continueButton.click();
    await flushTasks();

    assertFalse(page.getShouldShowWifiDebugLogsCheckboxForTesting());
  });

  // Test that the flag ShouldShowWifiDebugLogsCheckBox_ is true if
  // - is internal account.
  // - wifiDebugLogsAllowed is false.
  // - Wi-fi is mentioned in description.
  test('DoNotShowWifiDebugLogsCheckBox_NotAllowed', async () => {
    testWithInternalAccount();
    await initializePage();
    assertFalse(getFeedbackContext().wifiDebugLogsAllowed);
    assertFalse(page.getShouldShowWifiDebugLogsCheckboxForTesting());

    const searchPage = findChildElement(page, '.iron-selected');
    assertTrue(!!searchPage);
    assertEquals('searchPage', searchPage.id);

    const inputElement = findChildElement(searchPage as Element, 'textarea') as
        HTMLTextAreaElement;
    inputElement.value = 'wi-fi';
    // The flag ShouldShowWifiDebugLogsCheckBox_ is only updated when continue
    // button is clicked.
    const continueButton =
        findChildElement(searchPage as Element, '#buttonContinue') as
        CrButtonElement;
    continueButton.click();
    await flushTasks();

    assertFalse(page.getShouldShowWifiDebugLogsCheckboxForTesting());
  });

  // Test that the flag ShouldShowWifiDebugLogsCheckBox_ is true if
  // - is internal account.
  // - wifiDebugLogsAllowed is true.
  // - Wi-fi is mentioned in description.
  test('ShowWifiDebugLogsCheckBox', async () => {
    testWithInternalAccount();
    await initializePage();
    getFeedbackContext().wifiDebugLogsAllowed = true;
    assertFalse(page.getShouldShowWifiDebugLogsCheckboxForTesting());

    const searchPage = findChildElement(page, '.iron-selected');
    assertTrue(!!searchPage);
    assertEquals('searchPage', searchPage.id);

    const inputElement = findChildElement(searchPage as Element, 'textarea') as
        HTMLTextAreaElement;
    inputElement.value = 'wi-fi';
    // The flag ShouldShowWifiDebugLogsCheckBox_ is only updated when continue
    // button is clicked.
    const continueButton =
        findChildElement(searchPage as Element, '#buttonContinue') as
        CrButtonElement;
    continueButton.click();
    await flushTasks();

    assertTrue(page.getShouldShowWifiDebugLogsCheckboxForTesting());
  });

  // Test the "Link Cross Device Dogfood Feedback" checkbox will show up if
  // logged with internal account and input description is related.
  test(
      'ShowLinkCrossDeviceDogfoodFeedbackCheckboxsWithRelatedDescription',
      async () => {
        testWithInternalAccount();
        await initializePage();

        // Check the "Link Cross Device Dogfood Feedback" checkbox component is
        // hidden when input is not related to cross device.
        let activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('searchPage', activePage.id);

        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement)
            .value = 'abc';
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        loadTimeData.overrideValues(
            {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

        activePage = getActivePage();
        assertEquals('shareDataPage', activePage.id);
        const linkCrossDeviceDogfoodFeedbackCheckbox =
            activePage.shadowRoot!.querySelector(
                '#linkCrossDeviceDogfoodFeedbackCheckboxContainer');
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        assertFalse(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));

        strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        loadTimeData.overrideValues(
            {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

        // Go back to search page and set description input related to cross
        // device.
        activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('searchPage', activePage.id);

        // Testing tetherRegEx
        let descriptionElement = strictQuery(
            'textarea', activePage.shadowRoot, HTMLTextAreaElement);
        descriptionElement.value = 'hotspot';

        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('shareDataPage', activePage.id);

        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        assertTrue(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));

        strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        loadTimeData.overrideValues(
            {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

        // Go back to search page and set description input related to cross
        // device.
        activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('searchPage', activePage.id);

        // Testing phoneHubRegEx
        descriptionElement = strictQuery(
            'textarea', activePage.shadowRoot, HTMLTextAreaElement);
        descriptionElement.value = 'appstream';

        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('shareDataPage', activePage.id);

        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        assertTrue(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));

        strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        loadTimeData.overrideValues(
            {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

        // Go back to search page and set description input related to cross
        // device.
        activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('searchPage', activePage.id);

        // Testing phoneHubRegEx variation.
        descriptionElement = strictQuery(
            'textarea', activePage.shadowRoot, HTMLTextAreaElement);
        descriptionElement.value = 'camera roll';

        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        activePage = getActivePage();
        assertTrue(!!activePage);
        assertEquals('shareDataPage', activePage.id);

        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        assertTrue(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));
      });

  // Test the "Link Cross Device Dogfood Feedback" checkbox will not show up if
  // not logged with an Internal google account.
  test(
      'LinkCrossDeviceDogfoodFeedbackHiddenWithoutInternalAccount',
      async () => {
        await initializePage();

        // Enable flag.
        loadTimeData.overrideValues(
            {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

        // Set input description related to cross device.
        let activePage = getActivePage();
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement)
            .value = 'phone';
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        activePage = getActivePage();
        assertEquals('shareDataPage', activePage.id);
        const linkCrossDeviceDogfoodFeedbackCheckbox =
            activePage.shadowRoot!.querySelector(
                '#linkCrossDeviceDogfoodFeedbackCheckboxContainer');
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        assertFalse(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));
      });

  // Test the "Link Cross Device Dogfood Feedback" checkbox will not show up if
  // the ChromeOs device is not linked to a phone.
  test(
      'LinkCrossDeviceDogfoodFeedbackHiddenWithoutLinkedCrossDevicePhone',
      async () => {
        setupTestWithoutLinkedCrossDevicePhone();

        await initializePage();

        // Enable flag.
        loadTimeData.overrideValues(
            {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

        // Set input description related to cross device.
        let activePage = getActivePage();

        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement)
            .value = 'phone';
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        activePage = getActivePage();
        assertEquals('shareDataPage', activePage.id);
        const linkCrossDeviceDogfoodFeedbackCheckbox =
            activePage.shadowRoot!.querySelector(
                '#linkCrossDeviceDogfoodFeedbackCheckboxContainer');
        assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
        assertFalse(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));
      });

  // Test the "Link Cross Device Dogfood Feedback" checkbox will only show up
  // when 'enableLinkCrossDeviceDogfoodFeedbackFlag' is enabled.
  test('LinkCrossDeviceDogfoodFeedbackTestingFlag', async () => {
    testWithInternalAccount();
    await initializePage();

    // Enable flag and check that the checkbox appears.
    loadTimeData.overrideValues(
        {'enableLinkCrossDeviceDogfoodFeedbackFlag': true});

    // Set input description related to cross device.
    let activePage = getActivePage();

    strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement).value =
        'phone';
    strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    activePage = getActivePage();
    assertEquals('shareDataPage', activePage.id);
    const linkCrossDeviceDogfoodFeedbackCheckbox =
        activePage.shadowRoot!.querySelector(
            '#linkCrossDeviceDogfoodFeedbackCheckboxContainer');
    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    assertTrue(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));

    strictQuery('#buttonBack', activePage.shadowRoot, CrButtonElement).click();
    await flushTasks();

    // Go back to search page and set description input related to cross device.
    activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);

    const descriptionElement =
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement);
    descriptionElement.value = 'phone';

    // Disable flag and check that the checkbox doesn't appear.
    loadTimeData.overrideValues(
        {'enableLinkCrossDeviceDogfoodFeedbackFlag': false});

    strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('shareDataPage', activePage.id);

    assertTrue(!!linkCrossDeviceDogfoodFeedbackCheckbox);
    assertFalse(isVisible(linkCrossDeviceDogfoodFeedbackCheckbox));
  });

  // Test the assistant logs will show up if logged with internal account and
  // the fromAssistant flag is true.
  test('ShowAssistantCheckboxWithInternalAccountAndFlagSetTrue', async () => {
    // Replacing the query string to set the fromAssistant flag as true.
    setFromAssistantFlag(true);
    testWithInternalAccount();
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.SHARE_DATA);

    const feedbackContext = getFeedbackContext();
    assertTrue(feedbackContext.isInternalAccount);
    assertTrue(feedbackContext.fromAssistant);
    // Check the assistant checkbox component visible when input is not
    // related to bluetooth.
    const activePage = getActivePage();
    assertEquals('shareDataPage', activePage.id);

    const assistantCheckbox = strictQuery(
        '#assistantLogsContainer', activePage.shadowRoot, HTMLElement);

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

    const feedbackContext = getFeedbackContext();
    assertFalse(feedbackContext.isInternalAccount);
    assertTrue(feedbackContext.fromAssistant);
    let activePage = getActivePage();
    activePage = getActivePage();

    assertEquals('shareDataPage', activePage.id);
    const assistantCheckbox = strictQuery(
        '#assistantLogsContainer', activePage.shadowRoot, HTMLElement);
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

    const feedbackContext = getFeedbackContext();
    assertTrue(feedbackContext.isInternalAccount);
    assertFalse(feedbackContext.fromAssistant);
    // Set input description related to bluetooth.
    let activePage = getActivePage();
    activePage = getActivePage();

    assertEquals('shareDataPage', activePage.id);
    const assistantCheckbox = strictQuery(
        '#assistantLogsContainer', activePage.shadowRoot, HTMLElement);
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
        const feedbackContext = getFeedbackContext();
        assertTrue(feedbackContext.fromSettingsSearch);

        const activePage = getActivePage<SearchPageElement>();
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement)
            .value = 'text';
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        // Check the sys info and metrics checkbox component is unchecked when
        // the feedback app has opened through settings search
        const newActivePage = getActivePage<ShareDataPageElement>();
        assertEquals('shareDataPage', newActivePage.id);

        const sysInfoAndMetricsCheckboxContainer = strictQuery(
            '#sysInfoContainer', newActivePage.shadowRoot, HTMLElement);
        assertTrue(!!sysInfoAndMetricsCheckboxContainer);

        const sysInfoAndMetricsCheckbox = strictQuery(
            '#sysInfoCheckbox', newActivePage.shadowRoot, CrCheckboxElement);
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
        const feedbackContext = getFeedbackContext();
        assertFalse(feedbackContext.fromSettingsSearch);

        const activePage = getActivePage<SearchPageElement>();
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement)
            .value = 'text';
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement)
            .click();
        await flushTasks();

        const newActivePage = getActivePage<ShareDataPageElement>();
        assertEquals('shareDataPage', newActivePage.id);

        const sysInfoAndMetricsContainer = strictQuery(
            '#sysInfoContainer', newActivePage.shadowRoot, HTMLElement);
        assertTrue(!!sysInfoAndMetricsContainer);

        const sysInfoAndMetricsCheckbox = strictQuery(
            '#sysInfoCheckbox', newActivePage.shadowRoot, CrCheckboxElement);
        assertTrue(!!sysInfoAndMetricsCheckbox);
        assertTrue(sysInfoAndMetricsCheckbox.checked);
      });

  // Test the navigation from confirmation page to search page after the
  // send new report button is clicked.
  test('NavigateFromConfirmationPageToSearchPage', async () => {
    await initializePage();
    page.setCurrentStateForTesting(FeedbackFlowState.CONFIRMATION);
    // Set text input in search page for testing.
    const searchPage = getSearchPage();
    searchPage.setDescription(/*text=*/ 'abc123');

    const activePage = getActivePage<ConfirmationPageElement>();
    assertEquals('confirmationPage', activePage.id);

    const clickPromise = eventToPromise('go-back-click', page);

    let eventDetail: FeedbackFlowButtonClickEvent;
    page.addEventListener(
        'go-back-click', (event: FeedbackFlowButtonClickEvent) => {
          eventDetail = event;
        });

    strictQuery('#buttonNewReport', activePage.shadowRoot, CrButtonElement)
        .click();
    await clickPromise;

    assertEquals(
        FeedbackFlowState.CONFIRMATION, eventDetail!.detail!.currentState);

    // Should navigate to search page.
    const newActivePage = getActivePage<SearchPageElement>();
    assertEquals('searchPage', newActivePage.id);

    // Search text should be empty.
    const inputElement = getDescriptionElement();
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

    const activePage = getActivePage<ShareDataPageElement>();
    const shareDataPageSendButton =
        strictQuery('#buttonSend', activePage.shadowRoot, CrButtonElement);
    strictQuery('#buttonSend', activePage.shadowRoot, CrButtonElement).click();
    await continueClickPromise;

    // Should navigate to confirmation page.
    const confirmationPage = getActivePage<ConfirmationPageElement>();
    assertEquals('confirmationPage', confirmationPage.id);

    // The send button in the share data page should be disabled after
    // sending the report and before send new report button is clicked
    assertTrue(shareDataPageSendButton.disabled);

    // Click send new report button.
    strictQuery(
        '#buttonNewReport', confirmationPage!.shadowRoot, CrButtonElement)
        .click();
    await goBackClickPromise;

    // Should navigate to search page.
    const searchPage = getActivePage<SearchPageElement>();
    assertTrue(!!searchPage);
    assertEquals('searchPage', searchPage.id);

    // Add some text and clicks continue button.
    searchPage.setDescription(/*text=*/ 'abc123');
    strictQuery('#buttonContinue', searchPage!.shadowRoot, CrButtonElement)
        .click();
    await continueClickPromise;

    // Should navigate to share data page.
    const shareDataPage = getActivePage<ShareDataPageElement>();
    assertEquals('shareDataPage', shareDataPage.id);

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
        const fromAssistant = 'true';
        queryParams.set(
            AdditionalContextQueryParam.FROM_ASSISTANT, fromAssistant);
        const fromSettingsSearch = 'true';
        queryParams.set(
            AdditionalContextQueryParam.FROM_SETTINGS_SEARCH,
            fromSettingsSearch);
        // Replace current querystring with the new one.
        window.history.replaceState(null, '', '?' + queryParams.toString());
        await initializePage();
        page.setCurrentStateForTesting(FeedbackFlowState.SEARCH);
        const descriptionElement = getDescriptionElement();

        const feedbackContext = getFeedbackContext();
        assertEquals(page_url, feedbackContext.pageUrl!.url);
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
        const descriptionElement =
            findChildElement(getSearchPage(), '#descriptionText') as
            HTMLTextAreaElement;

        const feedbackContext = getFeedbackContext();
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
    const iframe =
        strictQuery('iframe', SearchPage.shadowRoot, HTMLIFrameElement);

    assertTrue(!!iframe);
    // Wait for the iframe completes loading.
    await eventToPromise('load', iframe);

    // There is another message posted from iframe which sends the height of
    // the help content.
    const expectedMessageEventCount = 2;
    let messageEventCount = 0;
    const resolver = new PromiseResolver<void>();

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
    iframe.contentWindow!.parent!.postMessage(data, OS_FEEDBACK_TRUSTED_ORIGIN);

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
    const activePage = getActivePage();
    assertTrue(!!activePage);
    assertEquals('searchPage', activePage.id);
    const inputElement =
        strictQuery('textarea', activePage.shadowRoot, HTMLTextAreaElement);
    const continueButton =
        strictQuery('#buttonContinue', activePage.shadowRoot, CrButtonElement);

    // Enter some text.
    inputElement.value = 'abc';
    continueButton.click();

    verifyHelpContentOutcomeMetricCalled(
        true, FeedbackAppHelpContentOutcome.kContinueNoHelpContentDisplayed);
  });

  test('UpdatesCSSUrl_TrustedUi', async () => {
    /*@type {HTMLLinkElement}*/
    const link = document.createElement('link');
    const disabledUrl = 'chrome://resources/chromeos/colors/cros_styles.css';
    link.href = disabledUrl;
    document.head.appendChild(link);

    await initializePage();

    const enabledUrl = 'theme/colors.css';
    assertTrue(link.href.includes(enabledUrl));

    // Clean up test specific element.
    document.head.removeChild(link);
  });

  // Test that test helper message is triggered on untrusted UI.
  test(
      'UpdatesCSSUrlBasedOn_UntrustedUi', async () => {
        const resolver = new PromiseResolver<void>();
        let colorChangeUpdaterCalled = false;
        const testMessageListener = (event: {data: {id: string}}) => {
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

  // Test that when dialog args is present, it will be used to populate the
  // feedback context.
  test('Create_feedback_context_from_dialogArguments_if_present', async () => {
    // Save the original chrome.getVariableValue function.
    const chromeGetVariableValue = chrome.getVariableValue;
    // Mock the chrome.getVariableValue to return dialogArguments.
    const mockChromeGetVariableValue = (message: string) => {
      if (message === 'dialogArguments') {
        return '{' +
            '"autofillMetadata":{"fake key1": "fake value1"},' +
            '"categoryTag":"Login",' +
            '"description":"fake description",' +
            '"descriptionPlaceholder":"fake description placeholder",' +
            '"fromAssistant": true, ' +
            '"fromAutofill": true, ' +
            '"fromSettingsSearch": true, ' +
            '"hasLinkedCrossDevicePhone": true, ' +
            '"isInternalAccount": true, ' +
            '"pageUrl":"chrome://flags/",' +
            '"systemInformation":[' +
            '  {' +
            '    "key": "EXTRA_DIAGNOSTICS",' +
            '    "value": "fake extra log data"' +
            '  }' +
            ']' +
            '}';
      }
      return '{}';
    };
    chrome.getVariableValue = mockChromeGetVariableValue;

    await initializePage();

    const feedbackContext = getFeedbackContext();
    assertEquals('Login', feedbackContext.categoryTag);
    assertEquals('fake extra log data', feedbackContext.extraDiagnostics);
    assertEquals('chrome://flags/', feedbackContext.pageUrl!.url);
    assertEquals(
        '{"fake key1":"fake value1"}', feedbackContext.autofillMetadata);
    assertTrue(feedbackContext.fromAssistant);
    assertTrue(feedbackContext.fromAutofill);
    assertTrue(feedbackContext.fromSettingsSearch);
    assertTrue(feedbackContext.hasLinkedCrossDevicePhone);
    assertTrue(feedbackContext.isInternalAccount);

    assertEquals('fake description', page.getDescriptionTemplateForTesting());
    assertEquals(
        'fake description placeholder',
        page.getDescriptionPlaceholderTextForTesting());
    assertFalse(page.getIsUserLoggedInForTesting());

    // Restore chrome.getVariableValue.
    chrome.getVariableValue = chromeGetVariableValue;
    // Verify that the getFeedbackContext is not called.
    assertEquals(0, feedbackServiceProvider.getFeedbackContextCallCount());
  });

  // Test that when dialog args is present, it will be used to populate the
  // feedback context. All fields are empty/absent.
  test(
      'Create_feedback_context_from_dialogArguments_if_present_empty',
      async () => {
        // Save the original chrome.getVariableValue function.
        const chromeGetVariableValue = chrome.getVariableValue;
        // Mock the chrome.getVariableValue to return dialogArguments.
        const mockChromeGetVariableValue = () => {
          return '{}';
        };
        chrome.getVariableValue = mockChromeGetVariableValue;

        await initializePage();

        const feedbackContext = getFeedbackContext();
        assertEquals('', feedbackContext.categoryTag);
        assertEquals('', feedbackContext.extraDiagnostics);
        assertEquals('', feedbackContext.pageUrl!.url);
        assertEquals('{}', feedbackContext.autofillMetadata);
        assertFalse(feedbackContext.fromAssistant);
        assertFalse(feedbackContext.fromAutofill);
        assertFalse(feedbackContext.fromSettingsSearch);
        assertFalse(feedbackContext.isInternalAccount);
        assertFalse(feedbackContext.hasLinkedCrossDevicePhone);

        assertTrue(!page.getDescriptionTemplateForTesting());
        assertTrue(!page.getDescriptionPlaceholderTextForTesting());
        assertTrue(page.getIsUserLoggedInForTesting());

        // Restore chrome.getVariableValue.
        chrome.getVariableValue = chromeGetVariableValue;

        // Verify that the getFeedbackContext is not called.
        assertEquals(0, feedbackServiceProvider.getFeedbackContextCallCount());
      });
});
