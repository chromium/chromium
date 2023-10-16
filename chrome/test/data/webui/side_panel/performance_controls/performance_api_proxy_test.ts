// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {PerformancePageCallbackRouter, PerformancePageHandlerRemote} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {PerformanceApiProxy, PerformanceApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/performance_api_proxy.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;
export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer!(mock);
  return mock;
}

suite('PerformanceApiProxyTest with mocked handler', () => {
  suite('with mocked handler', () => {
    let apiProxy: PerformanceApiProxy;
    let handlerMock: TestMock<PerformancePageHandlerRemote>;

    suiteSetup(async () => {
      handlerMock = installMock(
          PerformancePageHandlerRemote,
          (mock: PerformancePageHandlerRemote) => {
            PerformanceApiProxyImpl.setInstance(new PerformanceApiProxyImpl(
                new PerformancePageCallbackRouter(), mock));
          });

      apiProxy = PerformanceApiProxyImpl.getInstance();
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
      const apiProxy = PerformanceApiProxyImpl.getInstance();
      assertNotEquals(apiProxy, null);
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });
  });
});
