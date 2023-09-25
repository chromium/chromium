// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformancePageCallbackRouter} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {PerformanceApiProxy} from 'chrome://performance-side-panel.top-chrome/performance_api_proxy';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';


export class TestPerformanceApiProxy extends TestBrowserProxy implements
    PerformanceApiProxy {
  private callbackRouter_: PerformancePageCallbackRouter =
      new PerformancePageCallbackRouter();
  private callbackRouterRemote_ =
      this.callbackRouter_.$.bindNewPipeAndPassRemote();

  constructor() {
    super([]);
  }

  getCallbackRouter() {
    return this.callbackRouter_;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote_;
  }
}
