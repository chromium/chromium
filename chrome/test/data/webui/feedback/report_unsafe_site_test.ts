// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://feedback/report_unsafe_site/report_unsafe_site_app.js';

import {PageHandlerRemote} from 'chrome://feedback/report_unsafe_site.mojom-webui.js';
import type {ReportUnsafeSiteBrowserProxy} from 'chrome://feedback/report_unsafe_site/report_unsafe_site_browser_proxy.js';
import {ReportUnsafeSiteBrowserProxyImpl} from 'chrome://feedback/report_unsafe_site/report_unsafe_site_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestReportUnsafeSiteBrowserProxy implements ReportUnsafeSiteBrowserProxy {
  private handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  constructor() {
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.setScreenshot('data:image/png;base64,fakescreenshot');
  }

  getPageHandler() {
    return this.handler;
  }

  setScreenshot(dataUri: string) {
    this.handler.setPromiseResolveFor('getTriggeringPageInfo', {
      pageUrl: 'http://example.com',
      screenshotDataUri: dataUri,
    });
  }
}

suite('ReportUnsafeSiteTest', () => {
  let browserProxy: TestReportUnsafeSiteBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestReportUnsafeSiteBrowserProxy();
    ReportUnsafeSiteBrowserProxyImpl.setInstance(browserProxy);
  });

  test('ClickCancel', () => {
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    const cancelButton =
        app.shadowRoot.querySelector<HTMLInputElement>('#cancel-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    assertEquals(1, browserProxy.getPageHandler().getCallCount('closeDialog'));
  });

  test('ClickSend', async () => {
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    await browserProxy.getPageHandler().whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    const sendButton =
        app.shadowRoot.querySelector<HTMLInputElement>('.action-button');
    assertTrue(!!sendButton);
    sendButton.click();
    await browserProxy.getPageHandler().whenCalled('sendReport');
    assertEquals(1, browserProxy.getPageHandler().getCallCount('closeDialog'));
  });

  test('IncludeScreenshotCheckbox_HasScreenshot', async () => {
    browserProxy.setScreenshot(
        'data:image/png;data:image/png;base64,fakescreenshot');
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    const checkbox = app.$.includeScreenshotCheckbox;
    assertTrue(!!checkbox);
    await browserProxy.getPageHandler().whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    assertFalse(isChildVisible(app, '#screenshot-placeholder'));
    assertTrue(isChildVisible(app, '#screenshot-image'));

    // Click the checkbox to un-include the screenshot.
    checkbox.click();
    await eventToPromise('change', checkbox);
    assertFalse(checkbox.checked);
    assertTrue(isChildVisible(app, '#screenshot-placeholder'));
    assertFalse(isChildVisible(app, '#screenshot-image'));

    // Click the checkbox to include the screenshot.
    checkbox.click();
    await eventToPromise('change', checkbox);
    assertTrue(checkbox.checked);
    assertFalse(isChildVisible(app, '#screenshot-placeholder'));
    assertTrue(isChildVisible(app, '#screenshot-image'));
  });

  test('IncludeScreenshotCheckbox_NoScreenshot', async () => {
    browserProxy.setScreenshot('');
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);

    const checkbox = app.$.includeScreenshotCheckbox;
    assertTrue(!!checkbox);
    await browserProxy.getPageHandler().whenCalled('getTriggeringPageInfo');
    await microtasksFinished();
    assertFalse(checkbox.checked);
    assertTrue(isChildVisible(app, '#screenshot-placeholder'));
    assertFalse(isChildVisible(app, '#screenshot-image'));
  });
});
