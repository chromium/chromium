// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/system_info_app.js';
import 'chrome://feedback/strings.m.js';

import {FeedbackBrowserProxyImpl} from 'chrome://feedback/js/feedback_browser_proxy.js';
import type {SystemInfoAppElement} from 'chrome://feedback/system_info_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestFeedbackBrowserProxy} from './test_feedback_browser_proxy.js';

export const SYSTEM_LOGS: chrome.feedbackPrivate.LogsMapEntry[] = [
  {key: 'CHROME VERSION', value: '122.0.6261.94'},
  {key: 'OS VERSION', value: 'Linux'},
  {key: 'Related Website Sets', value: 'Disabled'},
];

suite('SystemInfoTest', function() {
  let app: SystemInfoAppElement;
  let browserProxy: TestFeedbackBrowserProxy;

  setup(function() {
    browserProxy = new TestFeedbackBrowserProxy();
    browserProxy.setSystemInfomation(SYSTEM_LOGS);
    FeedbackBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('system-info-app');
    document.body.appendChild(app);
  });

  test('RequestSystemInfoTest', function() {
    return browserProxy.whenCalled('getSystemInformation');
  });

  test('SystemInfoTitleTest', function() {
    assertEquals(
        loadTimeData.getString('sysinfoPageTitle'), app.$.title.textContent);
  });
});
