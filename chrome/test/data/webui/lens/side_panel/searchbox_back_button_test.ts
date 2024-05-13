// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

suite('SearchboxBackButton', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;

  setup(() => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
  });

  test('verify clicking back button calls browser proxy', async () => {
    lensSidePanelElement.isBackArrowVisible = true;
    await waitAfterNextRender(lensSidePanelElement);
    lensSidePanelElement.shadowRoot!.querySelector<HTMLElement>(
                                        '#backButton')!.click();
    return testBrowserProxy.handler.whenCalled('popAndLoadQueryFromHistory');
  });

  test('set searchbox back button visible', async () => {
    testBrowserProxy.page.setBackArrowVisible(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(
        isVisible(lensSidePanelElement.shadowRoot!.querySelector<HTMLElement>(
            '#backButton')!));
    testBrowserProxy.page.setBackArrowVisible(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(
        isVisible(lensSidePanelElement.shadowRoot!.querySelector<HTMLElement>(
            '#backButton')!));
  });
});
