// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NavigatorProxy} from 'chrome://privacy-sandbox-internals/navigator_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestNavigatorProxy extends TestBrowserProxy implements
    NavigatorProxy {
  constructor() {
    super(['writeToClipboard']);
  }

  writeToClipboard(text: string) {
    this.methodCalled('writeToClipboard', text);
    return Promise.resolve();
  }
}
