// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {BrowserProxyImpl, ContextMenuType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BatterySaverButtonElement} from 'chrome://webui-toolbar.top-chrome/app.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

suite('BatterySaverButton', function() {
  let button: BatterySaverButtonElement;
  let browserProxy: TestToolbarBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    button = document.createElement('battery-saver-button');
    document.body.appendChild(button);
  });

  test('ClickShowsBubble', async () => {
    assertEquals(
        0, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));

    assertTrue(!!button.shadowRoot, 'shadowRoot should not be null');
    const crIconButton = button.$.button;

    // Simulate click
    crIconButton.click();

    const [menuType] =
        await browserProxy.toolbarUIHandler.whenCalled('showContextMenu');
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
    assertEquals(ContextMenuType.kBatterySaver, menuType);
  });

  test('ShowsTooltip', () => {
    const crIconButton = button.$.button;
    // Tooltip and aria-label strings should be set
    assertEquals('Energy Saver is on', crIconButton.title);
    assertEquals('Energy Saver is on', crIconButton.getAttribute('aria-label'));
  });

  test('TabindexIsZero', () => {
    const crIconButton = button.$.button;
    assertEquals('0', crIconButton.getAttribute('tabindex'));
  });
});
