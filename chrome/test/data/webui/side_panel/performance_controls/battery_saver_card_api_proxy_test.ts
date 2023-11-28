// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/app.js';

import {BatterySaverCardApiProxy, BatterySaverCardApiProxyImpl} from 'chrome://performance-side-panel.top-chrome/battery_saver_card_api_proxy.js';
import {BatterySaverCardCallbackRouter, BatterySaverCardHandlerRemote} from 'chrome://performance-side-panel.top-chrome/performance.mojom-webui.js';
import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('BatterySaverCardApiProxyTest', () => {
  suite('with mocked handler', () => {
    let apiProxy: BatterySaverCardApiProxy;
    let handlerMock: TestMock<BatterySaverCardHandlerRemote>;

    suiteSetup(async () => {
      handlerMock = TestMock.fromClass(BatterySaverCardHandlerRemote);

      BatterySaverCardApiProxyImpl.setInstance(new BatterySaverCardApiProxyImpl(
          new BatterySaverCardCallbackRouter(),
          handlerMock as {} as BatterySaverCardHandlerRemote));

      apiProxy = BatterySaverCardApiProxyImpl.getInstance();
    });

    test('callback router is created', async () => {
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });
  });

  suite('without mocked handler', () => {
    test('getInstance constructs api proxy', async () => {
      const apiProxy = BatterySaverCardApiProxyImpl.getInstance();
      assertNotEquals(apiProxy, null);
      assertNotEquals(apiProxy.getCallbackRouter(), undefined);
    });
  });
});
