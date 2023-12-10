// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {MemorySaverCardApiProxy, MemorySaverCardApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/memory_saver_card_api_proxy.js';
import {MemorySaverCardCallbackRouter, MemorySaverCardHandlerRemote} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('MemorySaverCardApiProxyTest', () => {
  suite('with mocked handler', () => {
    let apiProxy: MemorySaverCardApiProxy;
    let handlerMock: TestMock<MemorySaverCardHandlerRemote>;

    suiteSetup(async () => {
      handlerMock = TestMock.fromClass(MemorySaverCardHandlerRemote);

      MemorySaverCardApiProxyImpl.setInstance(new MemorySaverCardApiProxyImpl(
          new MemorySaverCardCallbackRouter(),
          handlerMock as {} as MemorySaverCardHandlerRemote));

      apiProxy = MemorySaverCardApiProxyImpl.getInstance();
    });

    test('callback router is created', async () => {
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });
  });

  suite('without mocked handler', () => {
    test('getInstance constructs api proxy', async () => {
      const apiProxy = MemorySaverCardApiProxyImpl.getInstance();
      assertNotEquals(apiProxy, null);
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });
  });
});
