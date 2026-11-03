// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import {browserProxyFactory} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelPageHandler} from './test_side_panel_browser_proxy.js';

suite('SearchboxBackButton', () => {
  let handler: TestLensSidePanelPageHandler;
  let pageRemote: LensSidePanelPageRemote;
  let lensSidePanelElement: LensSidePanelAppElement;

  setup(() => {
    handler = new TestLensSidePanelPageHandler();
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    pageRemote = remote;

    // This test can only be run with the AIM searchbox disabled, since the
    // back button is removed when the AIM searchbox is enabled.
    loadTimeData.overrideValues({enableAimSearchbox: false});

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
  });

  test('verify clicking back button calls browser proxy', async () => {
    lensSidePanelElement.isBackArrowVisible = true;
    await waitAfterNextRender(lensSidePanelElement);
    lensSidePanelElement.shadowRoot!.querySelector<HTMLElement>(
                                        '#backButton')!.click();
    return handler.whenCalled('popAndLoadQueryFromHistory');
  });

  test('set searchbox back button visible', async () => {
    pageRemote.setBackArrowVisible(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(
        isVisible(lensSidePanelElement.shadowRoot!.querySelector<HTMLElement>(
            '#backButton')));
    pageRemote.setBackArrowVisible(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(
        isVisible(lensSidePanelElement.shadowRoot!.querySelector<HTMLElement>(
            '#backButton')));
  });
});
