// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ContentBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestContentBrowserProxy extends TestBrowserProxy implements
    ContentBrowserProxy {
  constructor() {
    super([
      'onConnected',
    ]);
  }

  onConnected(): void {
    this.methodCalled('onConnected');
  }
}
