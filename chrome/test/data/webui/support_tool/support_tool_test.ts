// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the SupportToolElement. It will be executed
 * by support_tool_browsertest.js.
 */

import 'chrome://support-tool/support_tool.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {BrowserProxy, BrowserProxyImpl} from 'chrome://support-tool/browser_proxy.js';
import {SupportToolElement} from 'chrome://support-tool/support_tool.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

const EMAIL_ADDRESSES: string[] =
    ['testemail1@test.com', 'testemail2@test.com'];

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
    ]);
  }

  getEmailAddresses() {
    this.methodCalled('getEmailAddresses');
    return Promise.resolve(EMAIL_ADDRESSES);
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

  test('initialize fields', () => {
    assertEquals(
        supportTool.shadowRoot!.querySelector('cr-input')!.value, 'testcaseid');
    const emailOptions = supportTool.shadowRoot!.querySelectorAll('option')!;
    // SupportToolElement adds empty string to the email addresses options as a
    // default value.
    assertEquals(EMAIL_ADDRESSES.length + 1, emailOptions.length);
  });
});
