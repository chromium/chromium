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
import {BrowserProxy, BrowserProxyImpl, DataCollectorItem, IssueDetails} from 'chrome://support-tool/browser_proxy.js';
import {SupportToolElement, SupportToolPageIndex} from 'chrome://support-tool/support_tool.js';
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

  startDataCollection(
      issueDetails: IssueDetails, selectedDataCollectors: DataCollectorItem[]) {
    this.methodCalled(
        'startDataCollection', [issueDetails, selectedDataCollectors]);
  }

  cancelDataCollection() {
    this.methodCalled('cancelDataCollection');
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
  });
});
