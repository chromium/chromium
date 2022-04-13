// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the SupportToolElement. It will be executed
 * by support_tool_browsertest.js.
 */

import 'chrome://support-tool/support_tool.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, BrowserProxyImpl, DataCollectorItem, IssueDetails, PIIDataItem} from 'chrome://support-tool/browser_proxy.js';
import {DataExportResult, SupportToolElement, SupportToolPageIndex} from 'chrome://support-tool/support_tool.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

const EMAIL_ADDRESSES: string[] =
    ['testemail1@test.com', 'testemail2@test.com'];

const DATA_COLLECTORS: DataCollectorItem[] = [
  {name: 'data collector 1', isIncluded: false, protoEnum: 1},
  {name: 'data collector 2', isIncluded: true, protoEnum: 2},
  {name: 'data collector 3', isIncluded: false, protoEnum: 3},
];

const PII_ITEMS: PIIDataItem[] = [
  {
    piiTypeDescription: 'IP Address',
    piiType: 0,
    detectedData: '255.255.155.2, 255.255.155.255, 172.11.5.5',
    count: 3,
    keep: false,
    expandDetails: true
  },
  {
    piiTypeDescription: 'Hash',
    piiType: 1,
    detectedData: '27540283740a0897ab7c8de0f809add2bacde78f',
    count: 1,
    keep: false,
    expandDetails: true
  },
  {
    piiTypeDescription: 'URL',
    piiType: 2,
    detectedData:
        'chrome://resources/f?user=bar, chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x, http://tets.com',
    count: 3,
    keep: false,
    expandDetails: true
  }
];

/**
 * A test version of SupportToolBrowserProxy.
 * Provides helper methods for allowing tests to know when a method was called,
 * as well as specifying mock responses.
 */
class TestSupportToolBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  constructor() {
    super([
      'getEmailAddresses',
      'getDataCollectors',
      'startDataCollection',
      'cancelDataCollection',
      'startDataExport',
      'showExportedDataInFolder',
      'getAllDataCollectors',
    ]);
  }

  getEmailAddresses() {
    this.methodCalled('getEmailAddresses');
    return Promise.resolve(EMAIL_ADDRESSES);
  }

  getDataCollectors() {
    this.methodCalled('getDataCollectors');
    return Promise.resolve(DATA_COLLECTORS);
  }

  getAllDataCollectors() {
    this.methodCalled('getAllDataCollectors');
    // TODO(b/217931906): For now, we return the same list of DATA_COLLECTORS.
    // Add a proper return value when we add test cases about
    // UrlGeneratorElement.
    return Promise.resolve(DATA_COLLECTORS);
  }

  startDataCollection(
      issueDetails: IssueDetails, selectedDataCollectors: DataCollectorItem[]) {
    this.methodCalled(
        'startDataCollection', [issueDetails, selectedDataCollectors]);
  }

  cancelDataCollection() {
    this.methodCalled('cancelDataCollection');
  }

  startDataExport(piiDataItems: PIIDataItem[]) {
    this.methodCalled('startDataExport', [piiDataItems]);
  }

  showExportedDataInFolder() {
    this.methodCalled('showExportedDataInFolder');
  }
}


suite('SupportToolTest', function() {
  let supportTool: SupportToolElement;
  let browserProxy: TestSupportToolBrowserProxy;

  const strings = {
    caseId: 'testcaseid',
  };

  setup(async function() {
    loadTimeData.overrideValues(strings);
    document.body.innerHTML = '';
    browserProxy = new TestSupportToolBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    supportTool = document.createElement('support-tool');
    document.body.appendChild(supportTool);
    await waitAfterNextRender(supportTool);
  });

  test('support tool pages navigation', () => {
    const ironPages = supportTool.shadowRoot!.querySelector('iron-pages');
    // The selected page index must be 0, which means initial page is
    // IssueDetails.
    assertEquals(ironPages!.selected, SupportToolPageIndex.ISSUE_DETAILS);
    // Only continue button container must be visible in initial page.
    assertFalse(
        supportTool.shadowRoot!.getElementById(
                                   'continueButtonContainer')!.hidden);
    // Click on continue button to move onto data collector selection page.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(
        ironPages!.selected, SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    // Click on continue button to start data collection.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    browserProxy.whenCalled('startDataCollection').then(function([
      issueDetails, selectedDataCollectors
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
    const emailOptions = issueDetails.shadowRoot!.querySelectorAll('option')!;
    // IssueDetailsElement adds empty string to the email addresses options as a
    // default value.
    assertEquals(EMAIL_ADDRESSES.length + 1, emailOptions.length);
  });

  test('data collector selection page', () => {
    // Check the contents of data collectors page.
    const ironListItems =
        supportTool.$.dataCollectors.shadowRoot!.querySelector(
                                                    'iron-list')!.items!;
    assertEquals(ironListItems.length, DATA_COLLECTORS.length);
    for (let i = 0; i < ironListItems.length; i++) {
      const listItem = ironListItems[i];
      assertEquals(listItem.name, DATA_COLLECTORS[i]!.name);
      assertEquals(listItem.isIncluded, DATA_COLLECTORS[i]!.isIncluded);
      assertEquals(listItem.protoEnum, DATA_COLLECTORS[i]!.protoEnum);
    }
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
          supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
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
        supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
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
        supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
        SupportToolPageIndex.EXPORT_SPINNER);
    const exportResult: DataExportResult = {
      success: true,
      path: '/usr/testuser/downloads/fake_support_packet_path.zip',
      error: ''
    };
    webUIListenerCallback('data-export-completed', exportResult);
    flush();
    assertEquals(
        supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
        SupportToolPageIndex.DATA_EXPORT_DONE);
  });
});
