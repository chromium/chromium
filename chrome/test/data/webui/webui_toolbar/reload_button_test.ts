// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {AnimationTracker, BrowserProxyImpl, ContextMenuType, PressHandler} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';
import type {BrowserProxy, ReloadButtonElement} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {TestToolbarUiHandler} from './test_toolbar_browser_proxy.js';
import {simulatePointerPress} from './test_util.js';

class TestBrowserControlsHandler extends TestBrowserProxy {
  constructor() {
    super(['reloadFromClick', 'stopLoad']);
  }

  reloadFromClick(bypassCache: boolean, flags: any, metadata: any) {
    this.methodCalled('reloadFromClick', [bypassCache, flags, metadata]);
  }

  stopLoad() {
    this.methodCalled('stopLoad');
  }
}

suite('ReloadButtonTest', function() {
  let reloadButton: ReloadButtonElement;
  let toolbarUiHandler: TestToolbarUiHandler;
  let browserControlsHandler: TestBrowserControlsHandler;
  let originalSetPointerCapture: any;
  let originalReleasePointerCapture: any;
  let originalHasPointerCapture: any;
  let originalBeginElement: any;

  // Pointer capture is mocked, since synthetic events have no active pointer
  // to capture. Captured pointers aren't tracked per-element, which is fine,
  // since only one button is pressed.
  const capturedPointers = new Set<number>();

  suiteSetup(function() {
    originalSetPointerCapture = HTMLElement.prototype.setPointerCapture;
    originalReleasePointerCapture = HTMLElement.prototype.releasePointerCapture;
    originalHasPointerCapture = HTMLElement.prototype.hasPointerCapture;
    HTMLElement.prototype.setPointerCapture = (id: number) => {
      capturedPointers.add(id);
    };
    HTMLElement.prototype.releasePointerCapture = (id: number) => {
      capturedPointers.delete(id);
    };
    HTMLElement.prototype.hasPointerCapture = (id: number) =>
        capturedPointers.has(id);

    // Prevent real SVG animations from playing to avoid timing flakes
    originalBeginElement = SVGAnimationElement.prototype.beginElement;
    SVGAnimationElement.prototype.beginElement = () => {};
  });

  suiteTeardown(function() {
    HTMLElement.prototype.setPointerCapture = originalSetPointerCapture;
    HTMLElement.prototype.releasePointerCapture = originalReleasePointerCapture;
    HTMLElement.prototype.hasPointerCapture = originalHasPointerCapture;
    SVGAnimationElement.prototype.beginElement = originalBeginElement;
  });

  setup(async function() {
    // Some tests leave pointers pressed.
    capturedPointers.clear();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbarUiHandler = new TestToolbarUiHandler();
    browserControlsHandler = new TestBrowserControlsHandler();
    BrowserProxyImpl.setInstance({
      toolbarUIHandler: toolbarUiHandler,
      browserControlsHandler: browserControlsHandler,
      recordInHistogram: () => {},
    } as unknown as BrowserProxy);

    loadTimeData.overrideValues({
      roundedIconsEnabled: true,
    });

    reloadButton = document.createElement('reload-button');
    document.body.appendChild(reloadButton);
    await reloadButton.updateComplete;
  });

  teardown(function() {
    AnimationTracker.resetForTesting();
  });

  function dispatchPointerEvent(
      type: 'pointerdown'|'pointermove'|'pointerup', yOffset: number = 0,
      pointerType: string = 'mouse') {
    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    const rect = button.getBoundingClientRect();
    button.dispatchEvent(new PointerEvent(type, {
      button: 0,
      pointerType,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2 + yOffset,
      bubbles: true,
      composed: true,
    }));
  }

  // Tests that the standard cr-icon-button ink drop ripple is suppressed
  // when Glow Up is enabled, since Glow Up defines its own interactive
  // hover/click animations.
  test('GlowUp Disables Ripple', async function() {
    AnimationTracker.showAnimations = true;
    // 1. Verify standard ripple is enabled by default (when glowUp is disabled)
    reloadButton.glowUpEnabled = false;
    await reloadButton.updateComplete;

    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    const ripple = button.getRipple();

    // Trigger a press to create the ripple
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
    }));

    assertTrue(button.hasRipple());
    assertEquals(1, ripple.shadowRoot.querySelectorAll('.ripple').length);

    // Clean up pointerdown
    button.dispatchEvent(new PointerEvent('pointerup', {
      button: 0,
      pointerType: 'mouse',
    }));

    // Reset body and recreate button to avoid cached ripple state issues
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    reloadButton = document.createElement('reload-button');
    document.body.appendChild(reloadButton);
    await reloadButton.updateComplete;

    // 2. Verify standard ripple is disabled when glowUp is enabled
    reloadButton.glowUpEnabled = true;
    await reloadButton.updateComplete;

    const buttonGlow = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    const rippleGlow = buttonGlow.getRipple();

    buttonGlow.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
    }));

    assertEquals(0, rippleGlow.shadowRoot.querySelectorAll('.ripple').length);
  });

  test('GlowUp Reload Animation Flow', async function() {
    AnimationTracker.showAnimations = true;
    reloadButton.glowUpEnabled = true;
    await reloadButton.updateComplete;

    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    // Default fallback icon when not animating.
    assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));

    simulatePointerPress(button);
    await browserControlsHandler.whenCalled('reloadFromClick');

    // Simulate Mojo updating state to loading. We must reassign the state
    // object to trigger Lit's reactive update, and change stateToken to force
    // updateState_ to run even though the update is initiated programmatically.
    (reloadButton as any).state = {
      ...(reloadButton as any).state,
      isNavigationLoading: true,
      stateToken: 1,
    };
    await reloadButton.updateComplete;
    // Yield to the macro-task queue (setTimeout 0) to allow the asynchronous
    // playAnimation_ callback (which runs on updateComplete.then) to execute
    // and set up the animation state before we assert.
    await new Promise(resolve => setTimeout(resolve, 0));

    // Should be animating 'start' (Reload -> Stop transition) now.
    assertEquals('start', (reloadButton as any).activeAnimation_);
    assertEquals(
        'webui-toolbar:reload_start_glow_up', button.getAttribute('iron-icon'));
    // The button is not disabled during the active animation unless the load
    // finishes early, so it should be false here.
    assertFalse((reloadButton as any).isDisabled);

    // Simulate animation end. We query the SMIL <animate> element inside the
    // cr-icon's shadow DOM and dispatch a synthetic 'endEvent' to simulate the
    // SVG animation finishing without waiting for real time.
    const animate =
        button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
            'animate[begin="indefinite"]')!;
    assertTrue(animate !== null);

    animate.dispatchEvent(new CustomEvent('endEvent'));
    await reloadButton.updateComplete;

    // Animation finished, should show stop icon (since isNavigationLoading is
    // true).
    assertEquals('none', (reloadButton as any).activeAnimation_);
    assertEquals('webui-toolbar:close', button.getAttribute('iron-icon'));
    assertFalse((reloadButton as any).isDisabled);
  });

  test('GlowUp Stop Animation Flow', async function() {
    AnimationTracker.showAnimations = true;
    reloadButton.glowUpEnabled = true;
    // Start in loading state (showing Stop icon).
    (reloadButton as any).state = {
      ...(reloadButton as any).state,
      isNavigationLoading: true,
    };
    await reloadButton.updateComplete;
    assertTrue((reloadButton as any).showStopIcon);

    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('webui-toolbar:close', button.getAttribute('iron-icon'));

    // Click it to stop.
    simulatePointerPress(button);
    await browserControlsHandler.whenCalled('stopLoad');

    // Should start 'end' (Stop -> Reload transition) animation immediately on
    // click.
    await reloadButton.updateComplete;
    // Yield to let the asynchronous playAnimation_ callback execute.
    await new Promise(resolve => setTimeout(resolve, 0));
    assertEquals('end', (reloadButton as any).activeAnimation_);
    assertEquals(
        'webui-toolbar:reload_end_glow_up', button.getAttribute('iron-icon'));
    // The button is not disabled during the 'end' animation when triggered by
    // explicit click, so it should be false.
    assertFalse((reloadButton as any).isDisabled);

    // Simulate animation end by dispatching synthetic 'endEvent'.
    const animate =
        button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
            'animate[begin="indefinite"]')!;
    animate.dispatchEvent(new CustomEvent('endEvent'));
    await reloadButton.updateComplete;

    // Animation finished, should show reload icon.
    assertEquals('none', (reloadButton as any).activeAnimation_);
    assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));
    assertFalse((reloadButton as any).isDisabled);
  });

  test('GlowUp Double Animation Flow', async function() {
    AnimationTracker.showAnimations = true;
    reloadButton.glowUpEnabled = true;
    await reloadButton.updateComplete;

    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));

    simulatePointerPress(button);
    await browserControlsHandler.whenCalled('reloadFromClick');

    // Simulate fast load: immediately set loading to false before the
    // update cycle runs.
    (reloadButton as any).state = {
      ...(reloadButton as any).state,
      isNavigationLoading: false,
    };
    await reloadButton.updateComplete;
    await new Promise(resolve => setTimeout(resolve, 0));

    // Should play double animation: start with 'start' and queue 'end'.
    assertEquals('start', (reloadButton as any).activeAnimation_);
    assertEquals('end', (reloadButton as any).pendingAnimation_);
    assertEquals(
        'webui-toolbar:reload_start_glow_up', button.getAttribute('iron-icon'));

    // Simulate 'start' animation end.
    const animate =
        button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
            'animate[begin="indefinite"]')!;
    assertTrue(animate !== null);

    animate.dispatchEvent(new CustomEvent('endEvent'));
    await reloadButton.updateComplete;
    await new Promise(resolve => setTimeout(resolve, 0));

    // Should transition to 'end' animation because it was pending.
    assertEquals('end', (reloadButton as any).activeAnimation_);
    assertEquals('none', (reloadButton as any).pendingAnimation_);
    assertEquals(
        'webui-toolbar:reload_end_glow_up', button.getAttribute('iron-icon'));

    // Simulate 'end' animation end.
    const animateEnd =
        button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
            'animate[begin="indefinite"]')!;
    animateEnd.dispatchEvent(new CustomEvent('endEvent'));
    await reloadButton.updateComplete;

    // Should return to idle reload state.
    assertEquals('none', (reloadButton as any).activeAnimation_);
    assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));
  });

  suite('LongPress', function() {
    const LONG_PRESS_MS = PressHandler.LONG_PRESS_TIMER_THRESHOLD_MS;
    const DRAG_DISTANCE_PX = PressHandler.DRAG_THRESHOLD_PX + 1;

    let mockTimer: MockTimer;

    setup(function() {
      mockTimer = new MockTimer();
      mockTimer.install();
    });

    teardown(function() {
      mockTimer.uninstall();
    });

    async function setCanShowMenu(canShowMenu: boolean) {
      reloadButton.state = {...reloadButton.state, canShowMenu};
      await reloadButton.updateComplete;
    }

    test('LongPressShowsMenuWhenAllowed', async function() {
      await setCanShowMenu(true);

      dispatchPointerEvent('pointerdown');
      mockTimer.tick(LONG_PRESS_MS);
      assertEquals(1, toolbarUiHandler.getCallCount('showContextMenu'));
      const [menuType, , source] =
          toolbarUiHandler.getArgs('showContextMenu')[0];
      assertEquals(ContextMenuType.kReload, menuType);
      assertEquals(MenuSourceType.kLongPress, source);

      dispatchPointerEvent('pointerup');
      assertEquals(0, browserControlsHandler.getCallCount('reloadFromClick'));
    });

    test('DragShowsMenuWhenAllowed', async function() {
      await setCanShowMenu(true);

      dispatchPointerEvent('pointerdown');
      dispatchPointerEvent('pointermove', DRAG_DISTANCE_PX);
      assertEquals(1, toolbarUiHandler.getCallCount('showContextMenu'));

      dispatchPointerEvent('pointerup');
      assertEquals(0, browserControlsHandler.getCallCount('reloadFromClick'));
    });

    // When the menu can't be shown, a long press should act like a normal
    // click.
    test('LongPressReloadsWhenMenuNotAllowed', async function() {
      await setCanShowMenu(false);

      dispatchPointerEvent('pointerdown');
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent('pointerup');

      assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
      assertEquals(1, browserControlsHandler.getCallCount('reloadFromClick'));
    });

    // When the menu can't be shown, dragging down then releasing over the
    // button should act like a normal click.
    test('DragReloadsWhenMenuNotAllowed', async function() {
      await setCanShowMenu(false);

      dispatchPointerEvent('pointerdown');
      dispatchPointerEvent('pointermove', DRAG_DISTANCE_PX);
      dispatchPointerEvent('pointerup');

      assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
      assertEquals(1, browserControlsHandler.getCallCount('reloadFromClick'));
    });

    // Matching Views, a long touch press should do nothing when the menu
    // can't be shown.
    test('TouchLongPressDoesNothingWhenMenuNotAllowed', async function() {
      await setCanShowMenu(false);

      dispatchPointerEvent('pointerdown', 0, 'touch');
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent('pointerup', 0, 'touch');

      assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
      assertEquals(0, browserControlsHandler.getCallCount('reloadFromClick'));
    });

    // If loading starts after the button is pressed, but before the long press
    // timer fires, the menu should not be shown, and releasing the button
    // should stop the load.
    test('LongPressStopsWhenLoadStartsDuringPress', async function() {
      await setCanShowMenu(true);

      dispatchPointerEvent('pointerdown');
      reloadButton.state = {
        ...reloadButton.state,
        canShowMenu: false,
        isNavigationLoading: true,
        stateToken: reloadButton.state.stateToken + 1,
      };
      await reloadButton.updateComplete;
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent('pointerup');

      assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
      assertEquals(1, browserControlsHandler.getCallCount('stopLoad'));
    });

    // The menu should never be shown while the stop icon is displayed, even
    // if `canShowMenu` is true. The browser never sends this state, but it
    // can happen when loading stops while the stop icon is pressed, since the
    // stop icon continues to be displayed while hovered. That's hard to
    // simulate here, so this test uses an artificial state instead.
    test('LongPressOnStopIconStops', async function() {
      reloadButton.state = {
        ...reloadButton.state,
        canShowMenu: true,
        isNavigationLoading: true,
      };
      await reloadButton.updateComplete;
      assertTrue((reloadButton as any).showStopIcon);

      dispatchPointerEvent('pointerdown');
      mockTimer.tick(LONG_PRESS_MS);
      dispatchPointerEvent('pointerup');

      assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
      assertEquals(1, browserControlsHandler.getCallCount('stopLoad'));
    });
  });

  test('GlowUp Respects Reduced Motion and AnimationTracker', async function() {
    const prefersReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    assertEquals(!prefersReducedMotion, AnimationTracker.showAnimations);

    let beginElementCalls = 0;
    const prevBeginElement = SVGAnimationElement.prototype.beginElement;
    SVGAnimationElement.prototype.beginElement = () => {
      ++beginElementCalls;
    };
    try {
      reloadButton.glowUpEnabled = true;
      await reloadButton.updateComplete;

      const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
      assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));

      // Default behavior follows @media (prefers-reduced-motion).
      simulatePointerPress(button);
      await browserControlsHandler.whenCalled('reloadFromClick');
      (reloadButton as any).state = {
        ...(reloadButton as any).state,
        isNavigationLoading: true,
        stateToken: 1,
      };
      await reloadButton.updateComplete;
      await new Promise(resolve => setTimeout(resolve, 0));

      if (prefersReducedMotion) {
        assertEquals('none', (reloadButton as any).activeAnimation_);
        assertEquals('none', (reloadButton as any).pendingAnimation_);
        assertEquals('webui-toolbar:close', button.getAttribute('iron-icon'));
        assertEquals(0, beginElementCalls);
      } else {
        assertEquals('start', (reloadButton as any).activeAnimation_);
        assertEquals(
            'webui-toolbar:reload_start_glow_up',
            button.getAttribute('iron-icon'));
        assertTrue(beginElementCalls > 0);
        const animate = button.shadowRoot.querySelector('cr-icon')!.shadowRoot
                            .querySelector('animate[begin="indefinite"]')!;
        animate.dispatchEvent(new CustomEvent('endEvent'));
        await reloadButton.updateComplete;
      }

      // Disabling animations via AnimationTracker suppresses stop -> reload
      // and fast-load double animations.
      AnimationTracker.showAnimations = false;
      beginElementCalls = 0;
      simulatePointerPress(button);
      await browserControlsHandler.whenCalled('stopLoad');
      await reloadButton.updateComplete;
      await new Promise(resolve => setTimeout(resolve, 0));

      assertEquals('none', (reloadButton as any).activeAnimation_);
      assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));
      assertEquals(0, beginElementCalls);

      browserControlsHandler.resetResolver('reloadFromClick');
      simulatePointerPress(button);
      await browserControlsHandler.whenCalled('reloadFromClick');
      (reloadButton as any).state = {
        ...(reloadButton as any).state,
        isNavigationLoading: false,
        stateToken: 2,
      };
      await reloadButton.updateComplete;
      await new Promise(resolve => setTimeout(resolve, 0));
      assertEquals('none', (reloadButton as any).activeAnimation_);
      assertEquals('none', (reloadButton as any).pendingAnimation_);
      assertEquals(0, beginElementCalls);

      // Enabling animations via AnimationTracker forces glow-up on even
      // when prefers-reduced-motion: reduce is active.
      AnimationTracker.showAnimations = true;
      browserControlsHandler.resetResolver('reloadFromClick');
      simulatePointerPress(button);
      await browserControlsHandler.whenCalled('reloadFromClick');
      (reloadButton as any).state = {
        ...(reloadButton as any).state,
        isNavigationLoading: true,
        stateToken: 3,
      };
      await reloadButton.updateComplete;
      await new Promise(resolve => setTimeout(resolve, 0));
      assertEquals('start', (reloadButton as any).activeAnimation_);
      assertEquals(
          'webui-toolbar:reload_start_glow_up',
          button.getAttribute('iron-icon'));
      assertTrue(beginElementCalls > 0);

      // Reset to idle reload state before testing mid-flight cancellation.
      const firstStartAnimate =
          button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
              'animate[begin="indefinite"]')!;
      firstStartAnimate.dispatchEvent(new CustomEvent('endEvent'));
      await reloadButton.updateComplete;
      AnimationTracker.showAnimations = false;
      (reloadButton as any).state = {
        ...(reloadButton as any).state,
        isNavigationLoading: false,
        stateToken: 4,
      };
      await reloadButton.updateComplete;

      // Disabling animations mid-double-animation suppresses the queued 'end'
      // animation when 'start' finishes.
      AnimationTracker.showAnimations = true;
      browserControlsHandler.resetResolver('reloadFromClick');
      simulatePointerPress(button);
      await browserControlsHandler.whenCalled('reloadFromClick');
      (reloadButton as any).state = {
        ...(reloadButton as any).state,
        isNavigationLoading: false,
        stateToken: 5,
      };
      await reloadButton.updateComplete;
      await new Promise(resolve => setTimeout(resolve, 0));
      assertEquals('start', (reloadButton as any).activeAnimation_);
      assertEquals('end', (reloadButton as any).pendingAnimation_);

      AnimationTracker.showAnimations = false;
      beginElementCalls = 0;
      const doubleStartAnimate =
          button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
              'animate[begin="indefinite"]')!;
      doubleStartAnimate.dispatchEvent(new CustomEvent('endEvent'));
      await reloadButton.updateComplete;
      await new Promise(resolve => setTimeout(resolve, 0));
      assertEquals('none', (reloadButton as any).activeAnimation_);
      assertEquals('none', (reloadButton as any).pendingAnimation_);
      assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));
      assertEquals(0, beginElementCalls);
    } finally {
      SVGAnimationElement.prototype.beginElement = prevBeginElement;
    }
  });
});
