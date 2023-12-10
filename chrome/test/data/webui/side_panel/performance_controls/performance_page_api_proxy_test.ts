// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {PerformancePageCallbackRouter, PerformancePageHandlerRemote} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {PerformancePageApiProxy, PerformancePageApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/performance_page_api_proxy.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('PerformancePageApiProxyTest', () => {
  suite('with mocked handler', () => {
    let apiProxy: PerformancePageApiProxy;
    let handlerMock: TestMock<PerformancePageHandlerRemote>;

    suiteSetup(async () => {
      handlerMock = TestMock.fromClass(PerformancePageHandlerRemote);

      PerformancePageApiProxyImpl.setInstance(new PerformancePageApiProxyImpl(
        new PerformancePageCallbackRouter(),
        handlerMock as {} as PerformancePageHandlerRemote));

      apiProxy = PerformancePageApiProxyImpl.getInstance();
    });

    test('callback router is created', async () => {
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });

    test('showUI call is passed through', async () => {
      apiProxy.showUi();
      assertEquals(1, handlerMock.getCallCount('showUI'));
    });
  });

  suite('without mocked handler', () => {
    test('getInstance constructs api proxy', async () => {
      const apiProxy = PerformancePageApiProxyImpl.getInstance();
      assertNotEquals(apiProxy, null);
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });
  });
});
