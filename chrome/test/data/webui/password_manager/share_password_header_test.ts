// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {OpenWindowProxyImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('SharePasswordHeaderTest', function() {
  let openWindowProxy: TestOpenWindowProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  test('HelpIconClickOpensSharingHelpUrl', async function() {
    const header = document.createElement('share-password-dialog-header');
    document.body.appendChild(header);
    flush();

    assertTrue(isVisible(header.$.helpButton));
    header.$.helpButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('passwordSharingLearnMoreURL'));
  });

  test('HelpIconClickOpensSharingTroubleshootHelpUrl', async function() {
    const header = document.createElement('share-password-dialog-header');
    header.isError = true;
    document.body.appendChild(header);
    flush();

    assertTrue(isVisible(header.$.helpButton));
    header.$.helpButton.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('passwordSharingTroubleshootURL'));
  });
});
