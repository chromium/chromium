// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/report_unsafe_site/report_unsafe_site_app.js';

import {PageHandlerRemote} from 'chrome://feedback/report_unsafe_site.mojom-webui.js';
import type {ReportUnsafeSiteBrowserProxy} from 'chrome://feedback/report_unsafe_site/report_unsafe_site_browser_proxy.js';
import {ReportUnsafeSiteBrowserProxyImpl} from 'chrome://feedback/report_unsafe_site/report_unsafe_site_browser_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

class TestReportUnsafeSiteBrowserProxy implements ReportUnsafeSiteBrowserProxy {
  private handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  constructor() {
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.handler.setPromiseResolveFor(
        'getPageUrl', {'pageUrl': 'http://example.com'});
  }

  getPageHandler() {
    return this.handler;
  }
}

suite('ReportUnsafeSiteTest', () => {
  test('ClickCancel', () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const browserProxy = new TestReportUnsafeSiteBrowserProxy();
    ReportUnsafeSiteBrowserProxyImpl.setInstance(browserProxy);
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    const cancelButton =
        app.shadowRoot.querySelector<HTMLInputElement>('.cancel-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    assertEquals(1, browserProxy.getPageHandler().getCallCount('closeDialog'));
  });
});
