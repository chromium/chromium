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

  test('SetTheme', async function() {
    const mockTheme = {
      borderColor: {value: 0xFFFF0000},
      borderGlowColor: {value: 0xFF0000FF},
      scrimColors: [
        {value: 0xFF00FF00},
        {value: 0xFFFFFF00},
        {value: 0xFF00FFFF},
      ],
    };
    testRemote.setTheme(mockTheme);
    await microtasksFinished();

    assertEquals(
        'rgba(255, 0, 0, 1.00)',
        page.style.getPropertyValue('--actor-border-color'));
    assertEquals(
        'rgba(0, 0, 255, 1.00)',
        page.style.getPropertyValue('--actor-border-glow-color'));
    assertEquals(
        'rgba(0, 255, 0, 1.00)',
        page.style.getPropertyValue('--actor-scrim-background-val1'));
    assertEquals(
        'rgba(255, 255, 0, 1.00)',
        page.style.getPropertyValue('--actor-scrim-background-val2'));
    assertEquals(
        'rgba(0, 255, 255, 1.00)',
        page.style.getPropertyValue('--actor-scrim-background-val3'));
  });

  test('MagicCursorDisabled', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    const point = {x: 100, y: 150};
    const movePromise = testRemote.moveCursorTo(point);
    await microtasksFinished();
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);
    await movePromise;
  });

  test('PreventsDefaultOnWheelEvent', async function() {
    // WheelEvent needs to be cancelable to allow e.preventDefault() to work.
    const wheelEvent = new WheelEvent('wheel', {cancelable: true});
    assertFalse(wheelEvent.defaultPrevented);
    page.dispatchEvent(wheelEvent);
    await microtasksFinished();
    assertTrue(wheelEvent.defaultPrevented);
  });
});

suite('BorderGlow', function() {
  let page: ActorOverlayAppElement;
  let testHandler: TestActorOverlayPageHandler;
  let testRemote: ActorOverlayPageRemote;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isMagicCursorEnabled: false,
      isStandaloneBorderGlowEnabled: true,
    });
    const testBrowserProxy = new TestActorOverlayBrowserProxy();
    ActorOverlayBrowserProxy.setInstance(testBrowserProxy);
    testHandler = testBrowserProxy.handler;
    testRemote = testBrowserProxy.remote;
  });

  setup(async function() {
    testHandler.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('actor-overlay-app');
    document.body.appendChild(page);
    await microtasksFinished();
  });

  teardown(function() {
    page.remove();
  });

  test('InitialState', function() {
    const borderGlow =
        page.shadowRoot.querySelector<HTMLElement>('#border-glow');
    assertTrue(!!borderGlow);
    assertTrue(borderGlow.parentElement!.hidden);
  });

  test('SetBorderGlowVisibility', async function() {
    const borderGlow =
        page.shadowRoot.querySelector<HTMLElement>('#border-glow');
    assertTrue(!!borderGlow);

    testRemote.setBorderGlowVisibility(true);
    await microtasksFinished();
    assertFalse(borderGlow.parentElement!.hidden);

    testRemote.setBorderGlowVisibility(false);
    await microtasksFinished();
    assertTrue(borderGlow.parentElement!.hidden);
  });

  test('InitialStateIsTrueWhenHandlerReturnsTrue', async function() {
    testHandler.setBorderGlowVisibilityForTesting(true);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const pageWithGlow = document.createElement('actor-overlay-app');
    document.body.appendChild(pageWithGlow);
    await microtasksFinished();

    const borderGlow =
        pageWithGlow.shadowRoot.querySelector<HTMLElement>('#border-glow');
    assertTrue(!!borderGlow);
    assertFalse(borderGlow.parentElement!.hidden);
  });

  test('DoesNotShowBorderGlowWhenFeatureIsDisabled', async function() {
    loadTimeData.overrideValues({isStandaloneBorderGlowEnabled: false});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const pageWithoutGlow = document.createElement('actor-overlay-app');
    document.body.appendChild(pageWithoutGlow);
    await microtasksFinished();

    const borderGlow =
        pageWithoutGlow.shadowRoot.querySelector<HTMLElement>('#border-glow');
    assertTrue(!!borderGlow);
    assertTrue(borderGlow.parentElement!.hidden);

    testRemote.setBorderGlowVisibility(true);
    await microtasksFinished();
    assertTrue(borderGlow.parentElement!.hidden);
  });
});

suite('MagicCursor', function() {
  let page: ActorOverlayAppElement;
  let testRemote: ActorOverlayPageRemote;
  // Store original DPR to restore it after tests.
  const originalDpr = window.devicePixelRatio;
  const originalInnerWindowWidth = window.innerWidth;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isMagicCursorEnabled: true,
    });
    const testBrowserProxy = new TestActorOverlayBrowserProxy();
    ActorOverlayBrowserProxy.setInstance(testBrowserProxy);
    testRemote = testBrowserProxy.remote;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('actor-overlay-app');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    // Restore original DPR.
    Object.defineProperty(window, 'devicePixelRatio', {
      writable: true,
      configurable: true,
      value: originalDpr,
    });
    // Restore original inner window width
    Object.defineProperty(window, 'innerWidth', {
      writable: true,
      configurable: true,
      value: originalInnerWindowWidth,
    });
  });

  test('MoveCursorAndVerifyLocation_StandardDPI', async function() {
    // Force 1.0 scale.
    Object.defineProperty(window, 'devicePixelRatio', {
      writable: true,
      configurable: true,
      value: 1.0,
    });

    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    // Input: 100, 150 (Physical)
    // Expected Output: 100 / 1.0 = 100px, 150 / 1.0 = 150px (Logical)
    const point = {x: 100, y: 150};
    const movePromise = testRemote.moveCursorTo(point);
    await microtasksFinished();
    assertEquals('translate(100px, 150px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise;
  });

  test('MoveCursorAndVerifyLocation_HighDPI', async function() {
    // Force 1.5 scale (High DPI).
    Object.defineProperty(window, 'devicePixelRatio', {
      writable: true,
      configurable: true,
      value: 1.5,
    });

    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    // Input: 150, 300 (Physical)
    // Expected Output: 150 / 1.5 = 100px, 300 / 1.5 = 200px (Logical)
    const point = {x: 150, y: 300};
    const movePromise = testRemote.moveCursorTo(point);
    await microtasksFinished();
    assertEquals('translate(100px, 200px)', magicCursor.style.transform);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise;
  });

  test('MoveCursorAndVerifyLocation_LowDPI', async function() {
    // Force 0.5 scale (Low DPI)
    Object.defineProperty(window, 'devicePixelRatio', {
      writable: true,
      configurable: true,
      value: 0.5,
    });

    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    // Input: 50, 100 (Physical)
    // Expected Output: 50 / 0.5 = 100px, 100 / 0.5 = 200px (Logical)
    const point = {x: 50, y: 100};
    const movePromise = testRemote.moveCursorTo(point);
    await microtasksFinished();
    assertEquals('translate(100px, 200px)', magicCursor.style.transform);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise;
  });

  test('MoveCursorTwiceAndVerifyLocation', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    const point = {x: 100, y: 150};
    const movePromise = testRemote.moveCursorTo(point);
    await microtasksFinished();
    assertEquals('translate(100px, 150px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise;

    const point2 = {x: 50, y: 100};
    const movePromise2 = testRemote.moveCursorTo(point2);
    await microtasksFinished();
    assertEquals('translate(50px, 100px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise2;
  });

  test('VerifyStyleProperties', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Verify default style
    const style = window.getComputedStyle(magicCursor);
    assertEquals(
        style.transitionTimingFunction, 'cubic-bezier(0.6, 0, 0.4, 1)');
    assertEquals(style.transitionProperty, 'transform');
    assertEquals(magicCursor.style.transitionDuration, '');

    /* Verify that cursor movements don't modify the transition animation
     * properties. */
    const point1 = {x: 100, y: 100};
    const movePromise1 = testRemote.moveCursorTo(point1);
    await microtasksFinished();
    assertEquals(
        style.transitionTimingFunction, 'cubic-bezier(0.6, 0, 0.4, 1)');
    assertEquals(style.transitionProperty, 'transform');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise1;

    const point2 = {x: 400, y: 100};
    const movePromise2 = testRemote.moveCursorTo(point2);
    await microtasksFinished();
    assertEquals(magicCursor.style.transitionDuration, '450ms');
    assertEquals(
        style.transitionTimingFunction, 'cubic-bezier(0.6, 0, 0.4, 1)');
    assertEquals(style.transitionProperty, 'transform');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise2;
  });

  test('DynamicDurationCalculation', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    // First cursor move.
    const point1 = {x: 100, y: 100};
    const movePromise1 = testRemote.moveCursorTo(point1);
    await microtasksFinished();
    assertEquals(magicCursor.style.transitionDuration, '212ms');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise1;

    // Start: (100, 100). Target: (400, 100). Distance: 300px.
    // Calculation: 300px / 0.667 px/ms = 450ms.
    const point2 = {x: 400, y: 100};
    const movePromise2 = testRemote.moveCursorTo(point2);
    await microtasksFinished();
    assertEquals(magicCursor.style.transitionDuration, '450ms');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise2;

    // Start: (400, 100). Target: (267, 233). Distance: hypot(133, 133) ≈ 188px.
    // Calculation: 188px / 0.667 px/ms ≈ 282ms.
    const point3 = {x: 267, y: 233};
    const movePromise3 = testRemote.moveCursorTo(point3);
    await microtasksFinished();
    assertEquals(magicCursor.style.transitionDuration, '282ms');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise3;

    // Start: (267, 233). Target: (270, 236). Distance: hypot(3, 3) ≈ 4.24px.
    // Natural Time: 4.24 / 0.667 ≈ 6.35ms. Expected Capped Time: 50ms.
    const point4 = {x: 270, y: 236};
    const movePromise4 = testRemote.moveCursorTo(point4);
    await microtasksFinished();
    assertEquals(magicCursor.style.transitionDuration, '50ms');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise4;

    // Start: (270, 236). Target: (1800, 236). Distance: 1530px.
    // Natural Time: 1530 / 0.667 ≈ 2293ms. Expected Capped Time: 675ms.
    const point5 = {x: 1800, y: 236};
    const movePromise5 = testRemote.moveCursorTo(point5);
    await microtasksFinished();
    assertEquals(magicCursor.style.transitionDuration, '675ms');
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise5;
  });

  async function verifyInitialCursorMove(
      targetX: number, targetY: number, expectedStartX: number,
      expectedStartY: number) {
    // Force innerWidth to be 1000px for testing.
    Object.defineProperty(window, 'innerWidth', {
      writable: true,
      configurable: true,
      value: 1000,
    });

    // Spy on the private `setCursorTransform` method to verify the sequence
    // of coordinate updates (edge start -> target). We call the original
    // method to ensure the UI still updates.
    const transformCalls: Array<{x: number, y: number}> = [];
    const originalSetTransform = (page as any).setCursorTransform.bind(page);
    (page as any).setCursorTransform = (x: number, y: number) => {
      transformCalls.push({x, y});
      originalSetTransform(x, y);
    };

    // Trigger Move.
    const point = {x: targetX, y: targetY};
    const movePromise = testRemote.moveCursorTo(point);
    await microtasksFinished();

    // Expect 2 calls: [0]: Start Position, [1]: End Position.
    assertEquals(2, transformCalls.length);

    // Check start position.
    assertEquals(expectedStartX, transformCalls[0]!.x);
    assertEquals(expectedStartY, transformCalls[0]!.y);

    // Check end position.
    assertEquals(targetX, transformCalls[1]!.x);
    assertEquals(targetY, transformCalls[1]!.y);

    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise;
  }

  test('InitialMoveFromLeftEdge', async function() {
    // Target (100, 100) is closer to left edge (0, 0).
    await verifyInitialCursorMove(100, 100, 0, 0);
  });

  test('InitialMoveFromRightEdge', async function() {
    // Target (900, 100) is closer to right edge (1000, 0).
    await verifyInitialCursorMove(900, 100, 1000, 0);
  });
});
