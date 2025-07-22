// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://actor-overlay/app.js';

import type {ActorOverlayAppElement} from 'chrome://actor-overlay/app.js';
import {ActorOverlayBrowserProxy} from 'chrome://actor-overlay/browser_proxy.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestActorOverlayPageHandler} from './test_browser_proxy.js';
import {TestActorOverlayBrowserProxy} from './test_browser_proxy.js';

suite('Scrim', function() {
  let page: ActorOverlayAppElement;
  let testHandler: TestActorOverlayPageHandler;

  setup(function() {
    const testBrowserProxy = new TestActorOverlayBrowserProxy();
    ActorOverlayBrowserProxy.setInstance(testBrowserProxy);
    testHandler = testBrowserProxy.handler;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('actor-overlay-app');
    document.body.appendChild(page);
  });

  test('PointerEnterAndLeave', async function() {
    page.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(await testHandler.whenCalled('onHoverStatusChanged'));
    testHandler.resetResolver('onHoverStatusChanged');
    page.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(await testHandler.whenCalled('onHoverStatusChanged'));
  });
});
