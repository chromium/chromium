// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

suite('SidePanelResultsFrame', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;

  setup(() => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
  });

  test('verify load results in frame works as expected', async () => {
    const url: Url = {url: 'https://www.google.com/'};
    testBrowserProxy.page.loadResultsInFrame(url);
    await flushTasks();
    assertEquals(lensSidePanelElement.$.results.src, url.url);
  });
});
