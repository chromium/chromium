// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://actor-overlay/app.js';

import type {ActorOverlayPageRemote} from 'chrome://actor-overlay/actor_overlay.mojom-webui.js';
import type {ActorOverlayAppElement} from 'chrome://actor-overlay/app.js';
import {ActorOverlayBrowserProxy} from 'chrome://actor-overlay/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
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
      magicCursorColor: {value: 0xFFFF00FF},
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
    assertEquals(
        'drop-shadow(0px 3px 5px rgba(255, 0, 255, 1.00))',
        page.style.getPropertyValue('--actor-magic-cursor-filter'));
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

// Cache the real setTimeout before MockTimer overrides it.
const realSetTimeout = window.setTimeout;

// microtasksFinished() uses setTimeout, which hangs indefinitely when MockTimer
// is installed. This bypasses MockTimer using the real setTimeout to safely
// flush pending mojo messages.
function flushTasks(): Promise<void> {
  return new Promise(resolve => realSetTimeout(resolve, 0));
}

suite('MagicCursor', function() {
  let page: ActorOverlayAppElement;
  let testRemote: ActorOverlayPageRemote;
  // Store original window params to restore it after tests.
  const originalDpr = window.devicePixelRatio;
  const originalInnerWindowWidth = window.innerWidth;
  const originalMatchMedia = window.matchMedia;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isMagicCursorEnabled: true,
      magicCursorSpeed: 0.667,
      magicCursorMinDurationMs: 50,
      magicCursorMaxDurationMs: 675,
    });
    const testBrowserProxy = new TestActorOverlayBrowserProxy();
    ActorOverlayBrowserProxy.setInstance(testBrowserProxy);
    testRemote = testBrowserProxy.remote;
  });

  setup(function() {
    // Force a standard 1.0 DPR for all tests.
    Object.defineProperty(window, 'devicePixelRatio', {
      writable: true,
      configurable: true,
      value: 1.0,
    });
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
    // Restore original window match media.
    window.matchMedia = originalMatchMedia;
  });

  async function moveCursorAndWait(x: number, y: number): Promise<void> {
    const movePromise = testRemote.moveCursorTo({x, y});
    await flushTasks();
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    magicCursor.dispatchEvent(new Event('transitionend'));
    await movePromise;
  }

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
    await moveCursorAndWait(100, 150);
    assertEquals('translate(100px, 150px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);
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
    await moveCursorAndWait(150, 300);
    assertEquals('translate(100px, 200px)', magicCursor.style.transform);
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
    await moveCursorAndWait(50, 100);
    assertEquals('translate(100px, 200px)', magicCursor.style.transform);
  });

  test('MoveCursorTwiceAndVerifyLocation', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    await moveCursorAndWait(100, 150);
    assertEquals('translate(100px, 150px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);

    await moveCursorAndWait(50, 100);
    assertEquals('translate(50px, 100px)', magicCursor.style.transform);
    assertEquals('1', magicCursor.style.opacity);
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
    await moveCursorAndWait(100, 100);
    assertEquals(
        style.transitionTimingFunction, 'cubic-bezier(0.6, 0, 0.4, 1)');
    assertEquals(style.transitionProperty, 'transform');

    await moveCursorAndWait(400, 100);
    assertEquals(magicCursor.style.transitionDuration, '450ms');
    assertEquals(
        style.transitionTimingFunction, 'cubic-bezier(0.6, 0, 0.4, 1)');
    assertEquals(style.transitionProperty, 'transform');
  });

  test('DynamicDurationCalculation', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);
    assertEquals('', magicCursor.style.opacity);
    assertEquals('', magicCursor.style.transform);

    // First cursor move.
    await moveCursorAndWait(100, 100);
    assertEquals(magicCursor.style.transitionDuration, '212ms');

    // Start: (100, 100). Target: (400, 100). Distance: 300px.
    // Calculation: 300px / 0.667 px/ms = 450ms.
    await moveCursorAndWait(400, 100);
    assertEquals(magicCursor.style.transitionDuration, '450ms');

    // Start: (400, 100). Target: (267, 233). Distance: hypot(133, 133) ≈ 188px.
    // Calculation: 188px / 0.667 px/ms ≈ 282ms.
    await moveCursorAndWait(267, 233);
    assertEquals(magicCursor.style.transitionDuration, '282ms');

    // Start: (267, 233). Target: (270, 236). Distance: hypot(3, 3) ≈ 4.24px.
    // Natural Time: 4.24 / 0.667 ≈ 6.35ms. Expected Capped Time: 50ms.
    await moveCursorAndWait(270, 236);
    assertEquals(magicCursor.style.transitionDuration, '50ms');

    // Start: (270, 236). Target: (1800, 236). Distance: 1530px.
    // Natural Time: 1530 / 0.667 ≈ 2293ms. Expected Capped Time: 675ms.
    await moveCursorAndWait(1800, 236);
    assertEquals(magicCursor.style.transitionDuration, '675ms');
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

    const isReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;

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
    await moveCursorAndWait(targetX, targetY);

    if (isReducedMotion) {
      // Expect ONLY 1 call (Directly to Target).
      assertEquals(1, transformCalls.length);
      assertEquals(targetX, transformCalls[0]!.x);
      assertEquals(targetY, transformCalls[0]!.y);
    } else {
      // Expect 2 calls: [0]: Start Position, [1]: End Position.
      assertEquals(2, transformCalls.length);

      // Check start position.
      assertEquals(expectedStartX, transformCalls[0]!.x);
      assertEquals(expectedStartY, transformCalls[0]!.y);

      // Check end position.
      assertEquals(targetX, transformCalls[1]!.x);
      assertEquals(targetY, transformCalls[1]!.y);
    }
  }

  test('InitialMoveFromLeftEdge', async function() {
    // Target (100, 100) is closer to left edge (0, 0).
    await verifyInitialCursorMove(100, 100, 0, 0);
  });

  test('InitialMoveFromRightEdge', async function() {
    // Target (900, 100) is closer to right edge (1000, 0).
    await verifyInitialCursorMove(900, 100, 1000, 0);
  });

  test('TriggerClickAnimation', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Move the cursor first to initialize it.
    await moveCursorAndWait(100, 150);

    // Trigger click animation.
    const clickPromise = testRemote.triggerClickAnimation();
    await microtasksFinished();

    // Verify the class is added and CSS variables are set.
    assertTrue(magicCursor.classList.contains('clicking'));
    assertEquals('100px', magicCursor.style.getPropertyValue('--cursor-x'));
    assertEquals('150px', magicCursor.style.getPropertyValue('--cursor-y'));

    // Finish animation and verify cleanup.
    magicCursor.dispatchEvent(new Event('animationend'));
    await clickPromise;
    assertFalse(magicCursor.classList.contains('clicking'));
  });

  test('TriggerClickAnimation_IgnoredIfUninitialized', async function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Ensure cursor is not initialized (opacity is 0).
    assertEquals('', magicCursor.style.opacity);

    // Attempt to click.
    const clickPromise = testRemote.triggerClickAnimation();
    await microtasksFinished();

    // Verify the animation class was never added because the cursor hasn't been
    // initialized by a movement first.
    assertFalse(magicCursor.classList.contains('clicking'));
    // The promise should still resolve immediately.
    await clickPromise;
  });

  test('ClickAnimationTimingMatchesCSS', function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    magicCursor.classList.add('clicking');
    const style = window.getComputedStyle(magicCursor);

    assertEquals('0.4s', style.animationDuration);
    assertEquals('ease-out', style.animationTimingFunction);
    assertTrue(style.animationName.includes('cursor-click'));
  });

  test('CursorImageStructure', function() {
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    const cursorImage = magicCursor.querySelector<HTMLElement>('#cursorImage');
    assertTrue(!!cursorImage);

    const style = window.getComputedStyle(cursorImage);
    assertEquals('transform', style.willChange);
    assertTrue(style.backgroundImage.includes('magic_cursor.svg'));
  });

  test('LoadingState_TriggersAfterMoveAndDelay', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Initialize the cursor with the first movement.
    await moveCursorAndWait(100, 100);

    // Verify that we aren't in the loading state yet.
    assertFalse(magicCursor.classList.contains('loading'));

    // Verify that we still aren't in the loading state yet after 100ms.
    mockTimer.tick(100);
    await page.updateComplete;
    assertFalse(magicCursor.classList.contains('loading'));

    // Wait another 150ms, total time is 250ms, which is greater than the 200ms
    // delay. Verify that we are now in the loading state.
    mockTimer.tick(150);
    await page.updateComplete;
    assertTrue(magicCursor.classList.contains('loading'));
    mockTimer.uninstall();
  });

  test('LoadingState_RemovedImmediatelyOnNewMove', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Initialize the cursor with the first movement.
    await moveCursorAndWait(100, 100);
    mockTimer.tick(250);  // Wait for delay
    await page.updateComplete;
    assertTrue(magicCursor.classList.contains('loading'));

    // Move the cursor again.
    await moveCursorAndWait(200, 200);

    // Verify the loading class is removed immediately.
    assertFalse(magicCursor.classList.contains('loading'));
    mockTimer.uninstall();
  });

  test('LoadingState_DelayCancelledByInterruption', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Initialize the cursor with the first movement.
    await moveCursorAndWait(100, 100);

    // Verify that we aren't in the loading state yet.
    mockTimer.tick(50);
    await page.updateComplete;
    assertFalse(magicCursor.classList.contains('loading'));

    // Trigger a click animation, which should kill the first loading timer.
    const clickPromise = testRemote.triggerClickAnimation();
    await flushTasks();

    // Wait another 200ms, total time would be 250ms. Verify that we are still
    // not in the loading state yet. This verifies that the first timer was
    // killed once the click animation was triggered.
    mockTimer.tick(200);
    await page.updateComplete;
    assertFalse(magicCursor.classList.contains('loading'));

    // Finish click animation
    magicCursor.dispatchEvent(new Event('animationend'));
    await clickPromise;

    // Wait 250ms, which should complete the new timer, verifying that we are in
    // the loading state.
    mockTimer.tick(250);
    await page.updateComplete;
    assertTrue(magicCursor.classList.contains('loading'));
    mockTimer.uninstall();
  });

  function setReducedMotion(enabled: boolean) {
    window.matchMedia = (query) => {
      return {
        matches: enabled && query === '(prefers-reduced-motion: reduce)',
        addListener: () => {},
        removeListener: () => {},
        addEventListener: () => {},
        removeEventListener: () => {},
      } as unknown as MediaQueryList;
    };
  }

  test('ReducedMotion_SpawnsAtTarget', async function() {
    setReducedMotion(true);
    await verifyInitialCursorMove(500, 500, 0, 0);
  });

  test('ReducedMotion_MoveCursor', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    setReducedMotion(true);
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Initialize cursor.
    await moveCursorAndWait(100, 100);

    // Trigger a second move.
    await moveCursorAndWait(800, 800);

    // Verify transition duration is 0ms.
    assertEquals('0ms', magicCursor.style.transitionDuration);
    assertEquals('translate(800px, 800px)', magicCursor.style.transform);

    // Verify that we don't enter loading state immediately after new cursor
    // location and after the 200ms delay.
    assertFalse(magicCursor.classList.contains('loading'));
    mockTimer.tick(250);
    await page.updateComplete;
    assertFalse(magicCursor.classList.contains('loading'));

    setReducedMotion(false);

    // Trigger a third move
    await moveCursorAndWait(100, 100);

    // Verify transition duration is restored (NOT 0ms).
    assertEquals('675ms', magicCursor.style.transitionDuration);
    assertEquals('translate(100px, 100px)', magicCursor.style.transform);

    // Verify loading state now appears after the delay.
    mockTimer.tick(250);
    await page.updateComplete;
    assertTrue(magicCursor.classList.contains('loading'));
    mockTimer.uninstall();
  });

  test('ReducedMotion_ClickAnimation', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    setReducedMotion(true);
    const magicCursor =
        page.shadowRoot.querySelector<HTMLElement>('#magicCursor');
    assertTrue(!!magicCursor);

    // Initialize cursor.
    await moveCursorAndWait(100, 100);

    // Trigger Click.
    const clickPromise = testRemote.triggerClickAnimation();
    await flushTasks();

    // Verify click still occurs.
    assertTrue(magicCursor.classList.contains('clicking'));
    magicCursor.dispatchEvent(new Event('animationend'));
    await clickPromise;
    assertFalse(magicCursor.classList.contains('clicking'));

    // Verify that we don't enter loading state immediately after new cursor
    // location and after the 200ms delay.
    assertFalse(magicCursor.classList.contains('loading'));
    mockTimer.tick(250);
    await page.updateComplete;
    assertFalse(magicCursor.classList.contains('loading'));
    mockTimer.uninstall();
  });

  test('ResizeIgnoredWhenCursorUninitialized', async function() {
    assertFalse(page.classList.contains('is-resizing'));
    window.dispatchEvent(new Event('resize'));
    await microtasksFinished();
    assertFalse(page.classList.contains('is-resizing'));
  });

  test('ResizeHidesCursorAndRestoresAfterDelay', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    // Initialize the cursor
    await moveCursorAndWait(100, 100);
    assertFalse(page.classList.contains('is-resizing'));

    window.dispatchEvent(new Event('resize'));
    await page.updateComplete;

    assertTrue(page.classList.contains('is-resizing'));
    mockTimer.tick(100);
    await page.updateComplete;
    assertTrue(page.classList.contains('is-resizing'));
    // Wait another 160ms (100 + 160 > 250ms)
    mockTimer.tick(160);
    await page.updateComplete;

    assertFalse(page.classList.contains('is-resizing'));
    mockTimer.uninstall();
  });

  test('ContinuousResizeKeepsCursorHidden', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    // Initialize the cursor
    await moveCursorAndWait(100, 100);
    assertFalse(page.classList.contains('is-resizing'));

    window.dispatchEvent(new Event('resize'));
    await page.updateComplete;
    assertTrue(page.classList.contains('is-resizing'));

    mockTimer.tick(150);
    await page.updateComplete;
    window.dispatchEvent(new Event('resize'));
    await page.updateComplete;
    assertTrue(page.classList.contains('is-resizing'));

    mockTimer.tick(150);
    await page.updateComplete;
    assertTrue(page.classList.contains('is-resizing'));

    // Wait past the final 250ms timer (150ms + 110ms = 260ms)
    mockTimer.tick(110);
    await page.updateComplete;
    assertFalse(page.classList.contains('is-resizing'));
    mockTimer.uninstall();
  });
});
