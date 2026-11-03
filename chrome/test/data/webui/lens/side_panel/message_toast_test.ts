// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens-overlay/lens_side_panel.mojom-webui.js';
import {browserProxyFactory} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelPageHandler} from './test_side_panel_browser_proxy.js';

suite('MessageToast', () => {
  let lensSidePanelElement: LensSidePanelAppElement;
  let callbackRouterRemote: LensSidePanelPageRemote;

  setup(() => {
    const {instance, remote} =
        browserProxyFactory.createForTest(new TestLensSidePanelPageHandler());
    browserProxyFactory.setInstance(instance);
    callbackRouterRemote = remote;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
  });

  test('ShowMessageToast', async () => {
    callbackRouterRemote.showToast('message');
    await waitAfterNextRender(lensSidePanelElement);

    assertTrue(isVisible(lensSidePanelElement.$.messageToast));
  });
});
