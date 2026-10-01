// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LocalStorageProxy} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestLocalStorageProxy extends TestBrowserProxy implements
    LocalStorageProxy {
  private items_: Map<string, string> = new Map();

  constructor() {
    super(['getItem', 'setItem']);
  }

  getItem(key: string): string|null {
    this.methodCalled('getItem', key);
    return this.items_.get(key) ?? null;
  }

  setItem(key: string, value: string) {
    this.methodCalled('setItem', key, value);
    this.items_.set(key, value);
  }
}
