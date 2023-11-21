// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformancePageCallbackRouter} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {PerformancePageApiProxy} from 'chrome://performance-side-panel.top-chrome/performance_page_api_proxy';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';


export class TestPerformancePageApiProxy extends TestBrowserProxy implements
    PerformancePageApiProxy {
  private callbackRouter: PerformancePageCallbackRouter =
      new PerformancePageCallbackRouter();

  constructor() {
    super([]);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  showUi() {}
}
