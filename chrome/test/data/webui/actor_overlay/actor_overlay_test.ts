// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://actor-overlay/app.js';

import type {ActorOverlayPageRemote} from 'chrome://actor-overlay/actor_overlay.mojom-webui.js';
import type {ActorOverlayAppElement} from 'chrome://actor-overlay/app.js';
import {ActorOverlayBrowserProxy} from 'chrome://actor-overlay/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import type {TestActorOverlayPageHandler} from './test_browser_proxy.js';
import {TestActorOverlayBrowserProxy} from './test_browser_proxy.js';

suite('Scrim', function() {
  let page: ActorOverlayAppElement;
  let testHandler: TestActorOverlayPageHandler;
  let testRemote: ActorOverlayPageRemote;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isMagicCursorEnabled: false,
    });
    const testBrowserProxy = new TestActorOverlayBrowserProxy();
    ActorOverlayBrowserProxy.setInstance(testBrowserProxy);
    testHandler = testBrowserProxy.handler;
    testRemote = testBrowserProxy.remote;
  });

  setup(function() {
    testHandler.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('actor-overlay-app');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('PointerEnterAndLeave', async function() {
    page.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(await testHandler.whenCalled('onHoverStatusChanged'));
    testHandler.resetResolver('onHoverStatusChanged');
    page.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(await testHandler.whenCalled('onHoverStatusChanged'));
  });

  test('SetScrimBackground', async function() {
    // Initial state should not contain the background-visible class.
    assertFalse(page.classList.contains('background-visible'));

    testRemote.setScrimBackground(true);
    await microtasksFinished();
    assertTrue(page.classList.contains('background-visible'));

    testRemote.setScrimBackground(false);
    await microtasksFinished();
    assertFalse(page.classList.contains('background-visible'));
  });

  test('MagicCursorDisabled', function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    const point = {x: 100, y: 150};
    page.moveCursorTo(point);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);
  });
});

suite('MagicCursor', function() {
  let page: ActorOverlayAppElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isMagicCursorEnabled: true,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('actor-overlay-app');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('MoveCursorAndVerifyLocation', function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    const point = {x: 100, y: 150};
    page.moveCursorTo(point);
    assertEquals('translate(100px, 150px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);

    const point2 = {x: 50, y: 100};
    page.moveCursorTo(point2);
    assertEquals('translate(50px, 100px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);
  });
});
