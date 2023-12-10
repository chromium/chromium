// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatterySaverCardApiProxy} from 'chrome://performance-side-panel.top-chrome/battery_saver_card_api_proxy';
import {BatterySaverCardCallbackRouter} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';


export class TestBatterySaverCardApiProxy extends TestBrowserProxy implements
    BatterySaverCardApiProxy {
  private callbackRouter: BatterySaverCardCallbackRouter =
      new BatterySaverCardCallbackRouter();

  constructor() {
    super([]);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }
}
