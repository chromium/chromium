// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OpenWindowProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOpenWindowProxy extends TestBrowserProxy implements
    OpenWindowProxy {
  constructor() {
    super(['openUrl']);
  }

  openUrl(url: string) {
    this.methodCalled('openUrl', url);
  }
}
