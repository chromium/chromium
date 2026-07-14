// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeUrlsData, PageHandlerInterface} from 'chrome://chrome-urls/chrome_urls.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test version of the PageHandler used to verify calls to the browser from
 * WebUI.
 */
export class TestPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private testData_: ChromeUrlsData = {
    webuiUrls: [],
    commandUrls: [],
    internalDebuggingUisEnabled: false,
  };

  constructor() {
    super(['getUrls', 'setDebugPagesEnabled']);
  }

  getUrls(): Promise<{urlsData: ChromeUrlsData}> {
    this.methodCalled('getUrls');
    return Promise.resolve({urlsData: structuredClone(this.testData_)});
  }

  setDebugPagesEnabled(enabled: boolean): Promise<void> {
    this.methodCalled('setDebugPagesEnabled', enabled);
    return Promise.resolve();
  }

  setTestData(data: ChromeUrlsData) {
    this.testData_ = structuredClone(data);
  }
}
