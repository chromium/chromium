// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/privacy_sandbox/app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';

import {assertEquals} from '../chai_assert.js';

import {TestOpenWindowProxy} from './test_open_window_proxy.js';

suite('PrivacySandbox', function() {
  /** @type {PrivacySandboxAppElement} */
  let page;

  /** @type {?TestOpenWindowProxy} */
  let openWindowProxy = null;

  setup(async function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.instance_ = openWindowProxy;

    document.body.innerHTML = '';
    page = /** @type {!PrivacySandboxAppElement} */
        (document.createElement('privacy-sandbox-app'));
    document.body.appendChild(page);
  });

  test('learnMoreTest', async function() {
    // User clicks the "Learn more" button.
    page.$$('#learnMoreButton').click();
    // Ensure the browser proxy call is done.
    assertEquals(
        loadTimeData.getString('privacySandboxURL'),
        await openWindowProxy.whenCalled('openURL'));
  });
});
