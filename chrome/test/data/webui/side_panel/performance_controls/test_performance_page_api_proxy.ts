// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformancePageCallbackRouter, PerformancePageHandlerRemote, PerformancePageRemote} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {PerformancePageApiProxy} from 'chrome://performance-side-panel.top-chrome/performance_page_api_proxy';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';


export class TestPerformancePageApiProxy extends TestBrowserProxy implements
    PerformancePageApiProxy {
  handler: TestMock<PerformancePageHandlerRemote>&PerformancePageHandlerRemote;
  observer: PerformancePageCallbackRouter;
  observerRemote: PerformancePageRemote;

  constructor() {
    super(['showUi']);
    this.handler = TestMock.fromClass(PerformancePageHandlerRemote);
    this.observer = new PerformancePageCallbackRouter();
    this.observerRemote = this.observer.$.bindNewPipeAndPassRemote();
  }

  getCallbackRouter() {
    return this.observer;
  }

  showUi() {
    this.methodCalled('showUi');
  }
}
