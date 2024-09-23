// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://system/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {AppElement} from 'chrome://system/app.js';
import {BrowserProxyImpl} from 'chrome://system/browser_proxy.js';
import type {SystemLog} from 'chrome://system/browser_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestAboutSysBrowserProxy} from './test_about_sys_browser_proxy.js';

export const SYSTEM_LOGS: SystemLog[] = [
  {statName: 'CHROME VERSION', statValue: '122.0.6261.94'},
  {statName: 'OS VERSION', statValue: 'Linux: 6.5.13-1rodete2-amd64'},
  {statName: 'Related Website Sets', statValue: 'Disabled'},
];

async function createAppElement(): Promise<AppElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const app = document.createElement('system-app');
  document.body.appendChild(app);
  await eventToPromise('ready-for-testing', app);
  return app;
}

suite('AboutSystemTest', function() {
  let app: AppElement;
  let browserProxy: TestAboutSysBrowserProxy;

  setup(async function() {
    browserProxy = new TestAboutSysBrowserProxy();
    browserProxy.setSystemLogs(SYSTEM_LOGS);
    BrowserProxyImpl.setInstance(browserProxy);
    app = await createAppElement();
  });

  test('RequestAboutSystemInfoTest', function() {
    return browserProxy.whenCalled('requestSystemInfo');
  });

  test('AboutSystemInfoTitleTest', function() {
    assertEquals(loadTimeData.getString('title'), app.$.title.textContent);
  });
});
