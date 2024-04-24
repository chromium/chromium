// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appParentalControlsHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type App = appParentalControlsHandlerMojom.App;
type AppParentalControlsHandlerInterface =
    appParentalControlsHandlerMojom.AppParentalControlsHandlerInterface;

export class FakeAppParentalControlsHandler extends TestBrowserProxy implements
    AppParentalControlsHandlerInterface {
  private apps_: App[];

  constructor() {
    super([
      'getApps',
    ]);

    this.apps_ = [];
  }

  getApps(): Promise<{apps: App[]}> {
    this.methodCalled('getApps');
    return Promise.resolve({apps: this.apps_});
  }

  addAppForTesting(app: App) {
    this.apps_.push(app);
  }
}
