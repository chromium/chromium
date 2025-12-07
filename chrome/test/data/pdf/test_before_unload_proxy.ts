// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import type {BeforeUnloadProxy} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {BeforeUnloadProxyImpl} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestBeforeUnloadProxy extends TestBrowserProxy implements
    BeforeUnloadProxy {
  constructor() {
    super(['preventDefault']);
  }

  preventDefault() {
    this.methodCalled('preventDefault');
  }
}

export function getNewTestBeforeUnloadProxy(): TestBeforeUnloadProxy {
  const testProxy = new TestBeforeUnloadProxy();
  BeforeUnloadProxyImpl.setInstance(testProxy);
  return testProxy;
}
