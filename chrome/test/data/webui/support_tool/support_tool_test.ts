// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the Support Tool UI elements. It will be
 * executed by support_tool_browsertest.js.
 */

import 'chrome://support-tool/support_tool.js';
import 'chrome://support-tool/url_generator.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {BrowserProxy, DataCollectorItem, IssueDetails, PiiDataItem, SupportTokenGenerationResult} from 'chrome://support-tool/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://support-tool/browser_proxy.js';
import type {ScreenshotElement} from 'chrome://support-tool/screenshot.js';
import type {DataExportResult, SupportToolElement} from 'chrome://support-tool/support_tool.js';
import {SupportToolPageIndex} from 'chrome://support-tool/support_tool.js';
import type {UrlGeneratorElement} from 'chrome://support-tool/url_generator.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {track} from 'chrome://webui-test/mouse_mock_interactions.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

const SCREENSHOT_BASE64: string =
    'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwME' +
    'AwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT' +
    '/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFB' +
    'QUFBQUFBQUFBQUFBQUFBT/wAARCABkAGQDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAA' +
    'AAAAAj/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFgEBAQEAAAAAAAAAAAAAAAAAAAcJ/8QA' +
    'FBEBAAAAAAAAAAAAAAAAAAAAAP/aAAwDAQACEQMRAD8AnQBDGqYAAAAAAAAAAAAAAAAAAAA' +
    'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
    'AAAAD/2Q==';

const EMAIL_ADDRESSES: string[] =
    ['testemail1@test.com', 'testemail2@test.com'];

const DATA_COLLECTORS: DataCollectorItem[] = [
  {name: 'data collector 1', isIncluded: false, protoEnum: 1},
  {name: 'data collector 2', isIncluded: true, protoEnum: 2},
  {name: 'data collector 3', isIncluded: false, protoEnum: 3},
];

const ALL_DATA_COLLECTORS: DataCollectorItem[] = [
  {name: 'data collector 1', isIncluded: false, protoEnum: 1},
  {name: 'data collector 2', isIncluded: false, protoEnum: 2},
  {name: 'data collector 3', isIncluded: false, protoEnum: 3},
  {name: 'data collector 4', isIncluded: false, protoEnum: 4},
  {name: 'data collector 5', isIncluded: false, protoEnum: 5},
];

const PII_ITEMS: PiiDataItem[] = [
  {
    piiTypeDescription: 'IP Address',
    piiType: 0,
    detectedData: ['255.255.155.2', '255.255.155.255', '172.11.5.5'],
    count: 3,
    keep: false,
    expandDetails: true,
  },
  {
    piiTypeDescription: 'Hash',
    piiType: 1,
    detectedData: ['27540283740a0897ab7c8de0f809add2bacde78f'],
    count: 1,
    keep: false,
    expandDetails: true,
  },
  {
    piiTypeDescription: 'URL',
    piiType: 2,
    detectedData: [
      'chrome://resources/f?user=bar',
      'chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x',
      'http://tets.com',
    ],
    count: 3,
    keep: false,
    expandDetails: true,
  },
];

/**
 * A test version of SupportToolBrowserProxy.
 * Provides helper methods for allowing tests to know when a method was called,
 * as well as specifying mock responses.
 */
class TestSupportToolBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  private supportTokenGenerationResult_: SupportTokenGenerationResult = {
    success: false,
    token: '',
    errorMessage: '',
  };

  constructor() {
    super([
      'takeScreenshot',
      'getEmailAddresses',
      'getDataCollectors',
      'startDataCollection',
      'cancelDataCollection',
      'startDataExport',
      'showExportedDataInFolder',
      'getAllDataCollectors',
      'generateCustomizedUrl',
      'generateSupportToken',
    ]);
  }

  takeScreenshot() {
    this.methodCalled('takeScreenshot');
  }

  getEmailAddresses() {
    this.methodCalled('getEmailAddresses');
    // We don't return EMAIL_ADDRESSES directly since we don't want the caller
    // to be able to modify the const array as it's possible in Typescript to
    // change the values of the contents of const arrays.
    return Promise.resolve(Array.from(EMAIL_ADDRESSES));
  }

  getDataCollectors() {
    this.methodCalled('getDataCollectors');
    return Promise.resolve(DATA_COLLECTORS);
  }

  getAllDataCollectors() {
    this.methodCalled('getAllDataCollectors');
    return Promise.resolve(ALL_DATA_COLLECTORS);
  }

  startDataCollection(
      issueDetails: IssueDetails, selectedDataCollectors: DataCollectorItem[],
      screenshot: string) {
    this.methodCalled(
        'startDataCollection', issueDetails, selectedDataCollectors,
        screenshot);
    // Return result with success for testing.
    const result = {success: true, errorMessage: ''};
    return Promise.resolve(result);
  }

  cancelDataCollection() {
    this.methodCalled('cancelDataCollection');
  }

  startDataExport(piiDataItems: PiiDataItem[]) {
    this.methodCalled('startDataExport', [piiDataItems]);
  }

  showExportedDataInFolder() {
    this.methodCalled('showExportedDataInFolder');
  }

  setSupportTokenGenerationResult(result: SupportTokenGenerationResult) {
    this.supportTokenGenerationResult_ = result;
  }

  // Returns this.supportTokenGenerationResult_ as response. Please call
  // this.setSupportTokenGenerationResult() before using this function in tests.
  generateCustomizedUrl(caseId: string, dataCollectors: DataCollectorItem[]) {
    this.methodCalled('generateCustomizedUrl', caseId, dataCollectors);
    return Promise.resolve(this.supportTokenGenerationResult_);
  }

  // Returns this.supportTokenGenerationResult_ as response. Please call
  // this.setSupportTokenGenerationResult() before using this function in tests.
  generateSupportToken(dataCollectors: DataCollectorItem[]) {
    this.methodCalled('generateSupportToken', dataCollectors);
    return Promise.resolve(this.supportTokenGenerationResult_);
  }
}

// Waits until `toast` is opened. Waits for 3 seconds and polls every second by
// default. Resolves to `true` if toast is opened. Returns error otherwise.
async function waitForToastOpened(
    toast: CrToastElement, intervalMs: number = 100,
    timeoutMs: number = 301): Promise<NonNullable<boolean>> {
  let rejectTimer: number|null = null;
  let pollTimer: number|null = null;

  function cleanup() {
    if (rejectTimer) {
      window.clearTimeout(rejectTimer);
    }
    if (pollTimer) {
      window.clearInterval(pollTimer);
    }
  }

  return new Promise((resolve, reject) => {
    rejectTimer = window.setTimeout(() => {
      cleanup();
      reject(new Error('Toast element did not get opened on time'));
    }, timeoutMs);

    pollTimer = window.setInterval(() => {
      if (toast.open) {
        cleanup();
        resolve(true);
      }
    }, intervalMs);
  });
}

suite('SupportToolTest', function() {
  let supportTool: SupportToolElement;
  let browserProxy: TestSupportToolBrowserProxy;

  const strings = {
    caseId: 'testcaseid',
  };

  setup(async function() {
    loadTimeData.overrideValues(strings);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSupportToolBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    supportTool = document.createElement('support-tool');
    document.body.appendChild(supportTool);
    await waitAfterNextRender(supportTool);
  });

  test('support tool pages navigation', () => {
    const pages = supportTool.shadowRoot!.querySelector('cr-page-selector');
    assertTrue(!!pages);

    // The selected page index must be 0, which means initial page is
    // IssueDetails.
    assertEquals(pages.selected, SupportToolPageIndex.ISSUE_DETAILS);
    // Only continue button container must be visible in initial page.
    assertFalse(
        supportTool.shadowRoot!.getElementById(
                                   'continueButtonContainer')!.hidden);
    // Click on continue button to move onto data collector selection page.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(pages.selected, SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    // Click on continue button to start data collection.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    browserProxy.whenCalled('startDataCollection').then(function([
      issueDetails,
      selectedDataCollectors,
    ]) {
      assertEquals(issueDetails.caseId, 'testcaseid');
      assertEquals(selectedDataCollectors, DATA_COLLECTORS);
    });
  });

  test('issue details page', () => {
    // Check the contents of data collectors page.
    const issueDetails = supportTool.$.issueDetails;
    assertEquals(
        issueDetails.shadowRoot!.querySelector('cr-input')!.value,
        'testcaseid');
    const emailOptions = issueDetails.shadowRoot!.querySelectorAll('option');
    // IssueDetailsElement adds DONT_INCLUDE_EMAIL string to the email addresses
    // options as for use to give the option to not include email address.
    assertEquals(EMAIL_ADDRESSES.length + 1, emailOptions.length);
  });

  test('data collector selection page', async () => {
    // Check the contents of data collectors page.
    const ironListItems =
        supportTool.$.dataCollectors.shadowRoot!.querySelector(
                                                    'dom-repeat')!.items!;
    assertEquals(ironListItems.length, DATA_COLLECTORS.length);
    for (let i = 0; i < ironListItems.length; i++) {
      const listItem = ironListItems[i];
      assertEquals(listItem.name, DATA_COLLECTORS[i]!.name);
      assertEquals(listItem.isIncluded, DATA_COLLECTORS[i]!.isIncluded);
      assertEquals(listItem.protoEnum, DATA_COLLECTORS[i]!.protoEnum);
    }

    const selectAllCheckbox =
        supportTool.$.dataCollectors.shadowRoot!.getElementById(
            'selectAllCheckbox')! as CrCheckboxElement;

    // Verify that the select all functionality works.
    selectAllCheckbox.click();
    await selectAllCheckbox.updateComplete;
    for (let i = 0; i < ironListItems.length; i++) {
      assertTrue(ironListItems[i].isIncluded);
    }

    // Verify that the unselect all functionality works.
    selectAllCheckbox.click();
    await selectAllCheckbox.updateComplete;
    for (let i = 0; i < ironListItems.length; i++) {
      assertFalse(ironListItems[i].isIncluded);
    }
  });

  test('take and remove screenshot', async () => {
    // Go to the data collector selection page.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-page-selector')!.selected,
        SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    // Take screenshot.
    const screenshot = supportTool.$.dataCollectors.shadowRoot!
                           .querySelector<ScreenshotElement>('#screenshot')!;
    const takeScreenshotButton =
        screenshot.shadowRoot!.getElementById('takeScreenshot')!;
    const removeButton =
        screenshot.shadowRoot!.getElementById('removeScreenshot')!;
    const hideInfoButton = screenshot.shadowRoot!.getElementById('hideInfo')!;
    takeScreenshotButton.click();
    await browserProxy.whenCalled('takeScreenshot');
    webUIListenerCallback('screenshot-received', SCREENSHOT_BASE64);
    assertFalse(removeButton.hidden);
    assertFalse(hideInfoButton.hidden);
    assertTrue(takeScreenshotButton.hidden);
    assertNotEquals('', screenshot.getEditedScreenshotBase64());
    // Remove screenshot.
    screenshot.shadowRoot!.getElementById('removeScreenshot')!.click();
    assertTrue(removeButton.hidden);
    assertTrue(hideInfoButton.hidden);
    assertFalse(takeScreenshotButton.hidden);
    assertEquals('', screenshot.getEditedScreenshotBase64());
  });

  test('take and edit screenshot', async () => {
    // Go to the data collector selection page.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-page-selector')!.selected,
        SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    // Take a screenshot.
    const screenshot = supportTool.$.dataCollectors.shadowRoot!
                           .querySelector<ScreenshotElement>('#screenshot')!;
    const takeScreenshotButton =
        screenshot.shadowRoot!.getElementById('takeScreenshot')!;
    const removeButton =
        screenshot.shadowRoot!.getElementById('removeScreenshot')!;
    const hideInfoButton = screenshot.shadowRoot!.getElementById('hideInfo')!;
    takeScreenshotButton.click();
    await browserProxy.whenCalled('takeScreenshot');
    webUIListenerCallback('screenshot-received', SCREENSHOT_BASE64);
    assertFalse(
        screenshot.shadowRoot!.getElementById('screenshotPreview')!.hidden);
    assertFalse(removeButton.hidden);
    assertFalse(hideInfoButton.hidden);
    assertTrue(takeScreenshotButton.hidden);
    assertNotEquals('', screenshot.getEditedScreenshotBase64());
    const originalScreenshot = screenshot.getOriginalScreenshotBase64();

    // Edit the screenshot.
    hideInfoButton.click();
    await waitAfterNextRender(screenshot);
    const canvas = screenshot.shadowRoot!.querySelector<HTMLCanvasElement>(
        '#screenshotCanvas')!;
    const confirmButton = screenshot.shadowRoot!.getElementById('confirmEdit')!;

    // After clicking the confirm button, the image is changed.
    track(canvas, canvas.width / 4, canvas.height / 4, 1);
    confirmButton.click();
    await waitAfterNextRender(screenshot);
    assertNotEquals(originalScreenshot, screenshot.getEditedScreenshotBase64());
  });

  test('spinner page', () => {
    // Check the contents of spinner page.
    const spinner = supportTool.$.spinnerPage;
    spinner.shadowRoot!.getElementById('cancelButton')!.click();
    browserProxy.whenCalled('cancelDataCollection').then(function() {
      webUIListenerCallback('data-collection-cancelled');
      flush();
      // Make sure the issue details page is displayed after cancelling data
      // collection.
      assertEquals(
          supportTool.shadowRoot!.querySelector('cr-page-selector')!.selected,
          SupportToolPageIndex.ISSUE_DETAILS);
    });
    assertEquals(browserProxy.getCallCount('cancelDataCollection'), 1);
  });

  test('PII selection page', async () => {
    // Go to the data collector selection page and start data collection by
    // clicking continue button twice so that the PII selection page gets
    // filled.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-page-selector')!.selected,
        SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    // Check the contents of PII selection page.
    const piiSelection = supportTool.$.piiSelection;
    browserProxy.whenCalled('startDataCollection').then(function() {
      webUIListenerCallback('data-collection-completed', PII_ITEMS);
      flush();
      const items =
          piiSelection.shadowRoot!.querySelector('dom-repeat')!.items!;
      assertEquals(items, PII_ITEMS);
    });
    assertEquals(browserProxy.getCallCount('startDataCollection'), 1);
    piiSelection.shadowRoot!.getElementById('exportButton')!.click();
    await browserProxy.whenCalled('startDataExport');
    webUIListenerCallback('support-data-export-started');
    flush();
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-page-selector')!.selected,
        SupportToolPageIndex.EXPORT_SPINNER);
    const exportResult: DataExportResult = {
      success: true,
      path: '/usr/testuser/downloads/fake_support_packet_path.zip',
      error: '',
    };
    webUIListenerCallback('data-export-completed', exportResult);
    flush();
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-page-selector')!.selected,
        SupportToolPageIndex.DATA_EXPORT_DONE);
  });
});

suite('UrlGeneratorTest', function() {
  let urlGenerator: UrlGeneratorElement;
  let browserProxy: TestSupportToolBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSupportToolBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    urlGenerator = document.createElement('url-generator');
    document.body.appendChild(urlGenerator);
    await waitAfterNextRender(urlGenerator);
  });

  test('url generation success', async () => {
    // Ensure the button is disabled when we open the page.
    const copyLinkButton = urlGenerator.shadowRoot!.getElementById(
                               'copyURLButton')! as CrButtonElement;
    assertTrue(copyLinkButton.disabled);
    const caseIdInput = urlGenerator.shadowRoot!.getElementById(
                            'caseIdInput')! as CrInputElement;
    caseIdInput.value = 'test123';
    const dataCollectors =
        urlGenerator.shadowRoot!.querySelectorAll('cr-checkbox');
    // Select one of data collectors to enable the button.
    const firstDataCollector = dataCollectors[0]! as CrCheckboxElement;
    firstDataCollector.click();
    await firstDataCollector.updateComplete;
    // Ensure the button is enabled after we select at least one data collector.
    assertFalse(copyLinkButton.disabled);
    const expectedToken = 'chrome://support-tool/?case_id=test123&module=jekhh';
    // Set the expected result of URL generation to successful.
    const expectedResult: SupportTokenGenerationResult = {
      success: true,
      token: expectedToken,
      errorMessage: '',
    };
    browserProxy.setSupportTokenGenerationResult(expectedResult);
    // Click the button to generate URL and copy to clipboard.
    copyLinkButton.click();
    await browserProxy.whenCalled('generateCustomizedUrl');
    // A toast message needs to be opened when the copy link button is clicked.
    assertTrue(await waitForToastOpened(urlGenerator.$.copyToast));
    // Check the URL value copied to clipboard if it's as expected.
    const copiedToken = await navigator.clipboard.readText();
    assertEquals(copiedToken, expectedToken);
  });

  test('url generation fail', async () => {
    // Set the expected result of URL generation to error.
    const expectedResult: SupportTokenGenerationResult = {
      success: false,
      token: '',
      errorMessage: 'Test error message',
    };
    browserProxy.setSupportTokenGenerationResult(expectedResult);
    const copyLinkButton = urlGenerator.shadowRoot!.getElementById(
                               'copyURLButton')! as CrButtonElement;
    // Enable the button for testing. The input fields are not important as
    // we're testing for the error message.
    copyLinkButton.disabled = false;
    // Click the button to generate URL.
    copyLinkButton!.click();
    await browserProxy.whenCalled('generateCustomizedUrl');
    // Check that there's an error message shown to user.
    assertTrue(await waitForToastOpened(urlGenerator.$.errorMessageToast));
  });

  test('token generation success', async () => {
    // Ensure the button is disabled when we open the page.
    const copyTokenButton = urlGenerator.shadowRoot!.getElementById(
                                'copyTokenButton')! as CrButtonElement;
    assertTrue(copyTokenButton.disabled);
    const dataCollectors =
        urlGenerator.shadowRoot!.querySelectorAll('cr-checkbox');
    // Select one of data collectors to enable the button.
    const firstDataCollector = dataCollectors[0]! as CrCheckboxElement;
    firstDataCollector.click();
    await firstDataCollector.updateComplete;
    // Ensure the button is enabled after we select at least one data collector.
    assertFalse(copyTokenButton.disabled);
    const expectedToken = 'jekhh';
    // Set the expected result of token generation to successful.
    const expectedResult: SupportTokenGenerationResult = {
      success: true,
      token: expectedToken,
      errorMessage: '',
    };
    browserProxy.setSupportTokenGenerationResult(expectedResult);
    // Click the button to generate URL and copy to clipboard.
    copyTokenButton.click();
    await browserProxy.whenCalled('generateSupportToken');
    // A toast message needs to be opened when the copy token button is clicked.
    assertTrue(await waitForToastOpened(urlGenerator.$.copyToast));
    // Check the token value copied to clipboard if it's as expected.
    const copiedToken = await navigator.clipboard.readText();
    assertEquals(copiedToken, expectedToken);
  });
});
