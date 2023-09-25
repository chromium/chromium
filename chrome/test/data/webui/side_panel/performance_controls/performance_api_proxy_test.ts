// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {PerformanceApiProxy, PerformanceApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/performance_api_proxy.js';
import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('PerformanceApiProxyTest', () => {
  let apiProxy: PerformanceApiProxy;

  setup(async () => {
    apiProxy = new PerformanceApiProxyImpl();
  });

  test('callback router is created', async () => {
    assertNotEquals(apiProxy.getCallbackRouter(), undefined);
  });
});
