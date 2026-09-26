// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, ContentSettingImageType, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ContentSettingIconElement} from 'chrome://webui-toolbar.top-chrome/app.js';

import {TestToolbarUiHandler} from './test_toolbar_browser_proxy.js';

const COLLAPSE_HOLD_DURATION_MS = 1800;
const AUTO_COLLAPSE_DELAY_MS = 2400;
const REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS = 3000;

suite('ContentSettingIcon', function() {
  let icon: ContentSettingIconElement;
  let handler: TestToolbarUiHandler;
  let startTrackingCalls: Array<[HTMLElement, string]> = [];
  let stopTrackingCalls: HTMLElement[] = [];

  setup(async () => {
    handler = new TestToolbarUiHandler();
    BrowserProxyImpl.setInstance({toolbarUIHandler: handler} as any);

    const trustedTypes = window.trustedTypes!;
    document.body.innerHTML = trustedTypes.emptyHTML;
    startTrackingCalls = [];
    stopTrackingCalls = [];

    const mockManager: Partial<TrackedElementManager> = {
      startTracking: (element: HTMLElement, nativeId: string) => {
        startTrackingCalls.push([element, nativeId]);
      },
      stopTracking: (element: HTMLElement) => {
        stopTrackingCalls.push(element);
      },
    };
    TrackedElementManager.setInstance(mockManager as TrackedElementManager);

    icon = document.createElement('content-setting-icon');
    icon.state = {
      type: ContentSettingImageType.kCookies,
      isBlocked: false,
      tooltip: 'Tooltip',
      accessibilityString: 'Accessible Name',
      shouldRunAnimation: false,
      explanatoryString: '',
      identifier: {
        nativeIdentifier: '',
        secondaryIdentifier: '',
      },
    };
    document.body.appendChild(icon);

    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();
  });

  test('ARIA label', () => {
    const innerButton = icon.$.chip.$.button;
    assertTrue(!!innerButton);
    assertEquals('Accessible Name', innerButton.getAttribute('aria-label'));
  });

  test('Icon name rendering', async () => {
    const iconElement = icon.shadowRoot.querySelector<CrIconElement>('#icon');
    assertTrue(!!iconElement);
    assertEquals('webui-toolbar-shared:database', iconElement.icon);

    icon.state = {
      ...icon.state,
      isBlocked: true,
    };
    await microtasksFinished();
    assertEquals('webui-toolbar-shared:database_off', iconElement.icon);
  });

  test('Animation', async () => {
    assertFalse(icon.hasAttribute('should-show-label'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: 'Blocked',
    };
    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();
    assertTrue(icon.hasAttribute('should-show-label'));
    assertTrue(icon.$.chip.hasAttribute('has-label'));
    const isReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    if (isReducedMotion) {
      assertEquals(0, icon.$.label.getAnimations().length);
    } else {
      assertGT(icon.$.label.getAnimations().length, 0);
    }
  });

  test('SpuriousUpdateDuringAnimation', async () => {
    assertFalse(icon.hasAttribute('should-show-label'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: 'Blocked',
    };
    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();
    assertTrue(icon.hasAttribute('should-show-label'));
    assertTrue(icon.$.chip.hasAttribute('has-label'));
    const isReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    if (isReducedMotion) {
      assertEquals(0, icon.$.label.getAnimations().length);
    } else {
      assertGT(icon.$.label.getAnimations().length, 0);
    }

    // Real C++ behavior instantly sets (and pushes) shouldRunAnimation to false
    // for subsequent updates while the icon is open.
    icon.state = {
      ...icon.state,
      shouldRunAnimation: false,
    };
    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();
    // Spurious update should NOT cancel the in-progress JS timer.
    assertTrue(icon.hasAttribute('should-show-label'));
    assertTrue(icon.$.chip.hasAttribute('has-label'));
    if (isReducedMotion) {
      assertEquals(0, icon.$.label.getAnimations().length);
    } else {
      assertGT(icon.$.label.getAnimations().length, 0);
    }
  });

  test('NoAnimationWithoutExplanatoryString', async () => {
    assertFalse(icon.hasAttribute('should-show-label'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: '',
    };
    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();
    assertFalse(icon.hasAttribute('should-show-label'));
    assertFalse(icon.$.chip.hasAttribute('has-label'));
    assertEquals(0, icon.$.label.getAnimations().length);
  });

  test('CollapseTimerExpires', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(icon.hasAttribute('should-show-label'));
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // Advance to trigger the collapse timer.
      const isReducedMotion =
          window.matchMedia('(prefers-reduced-motion: reduce)').matches;
      const expectedDelay = isReducedMotion ?
          REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS :
          AUTO_COLLAPSE_DELAY_MS;
      mockTimer.tick(expectedDelay - 1);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      mockTimer.tick(1);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));
    } finally {
      mockTimer.uninstall();
    }
  });

  test('BubbleOpenPreventsCollapseAndCloseResumes', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(icon.hasAttribute('should-show-label'));
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // Bubble opens before collapse timer expires.
      icon.trackedHighlighted = true;
      await icon.updateComplete;

      // Advancing past original collapse duration should NOT collapse the
      // label.
      mockTimer.tick(REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS + 1000);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // Bubble closes: should start a COLLAPSE_HOLD_DURATION_MS collapse timer.
      icon.trackedHighlighted = false;
      await icon.updateComplete;
      mockTimer.tick(COLLAPSE_HOLD_DURATION_MS - 1);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      mockTimer.tick(1);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));
    } finally {
      mockTimer.uninstall();
    }
  });

  test('PointerdownPreventsCollapseAndPointerupResumes', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(icon.hasAttribute('should-show-label'));
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // User presses mouse down on the chip.
      icon.$.chip.dispatchEvent(new PointerEvent('pointerdown', {button: 0}));
      assertEquals(1, handler.getCallCount('onContentSettingImagePointerDown'));

      // Advancing time should NOT collapse the label while mouse is pressed.
      mockTimer.tick(REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS + 1000);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // User releases mouse without bubble opening.
      icon.$.chip.dispatchEvent(new PointerEvent('pointerup', {button: 0}));
      mockTimer.tick(COLLAPSE_HOLD_DURATION_MS - 1);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      mockTimer.tick(1);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));
    } finally {
      mockTimer.uninstall();
    }
  });

  test('PointercancelResumesCollapseTimer', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(icon.hasAttribute('should-show-label'));
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      icon.$.chip.dispatchEvent(new PointerEvent('pointerdown', {button: 0}));
      icon.$.chip.dispatchEvent(new PointerEvent('pointercancel'));
      mockTimer.tick(COLLAPSE_HOLD_DURATION_MS - 1);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      mockTimer.tick(1);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));
    } finally {
      mockTimer.uninstall();
    }
  });

  test('DisconnectedCleansUpTimer', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      icon.remove();
      mockTimer.tick(3000);
      await icon.updateComplete;
      // No errors should occur on timer expiry when disconnected.
    } finally {
      mockTimer.uninstall();
    }
  });

  test('ClickWhenCollapsedDoesNotExpandLabel', async () => {
    assertFalse(icon.hasAttribute('should-show-label'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: false,
      explanatoryString: 'Blocked',
    };
    await microtasksFinished();

    icon.$.chip.dispatchEvent(new PointerEvent('pointerdown', {button: 0}));
    await microtasksFinished();
    assertFalse(icon.hasAttribute('should-show-label'));

    icon.$.chip.dispatchEvent(new PointerEvent('click'));
    await microtasksFinished();
    assertFalse(icon.hasAttribute('should-show-label'));
    assertEquals(1, handler.getCallCount('showContentSettingsBubble'));
  });

  test('HoverDoesNotPreventCollapse', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(icon.hasAttribute('should-show-label'));
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // Hovering over the expanding chip does NOT pause or prevent collapse
      // (matching native views parity).
      icon.$.chip.dispatchEvent(new PointerEvent('pointerenter'));

      // Advancing time past collapse duration should collapse the label even
      // while hovered.
      const isReducedMotion =
          window.matchMedia('(prefers-reduced-motion: reduce)').matches;
      const expectedDelay = isReducedMotion ?
          REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS :
          AUTO_COLLAPSE_DELAY_MS;
      mockTimer.tick(expectedDelay - 1);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      mockTimer.tick(1);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));

      // Moving the mouse away after collapse should not re-expand or restart.
      icon.$.chip.dispatchEvent(new PointerEvent('pointerleave'));
      mockTimer.tick(COLLAPSE_HOLD_DURATION_MS);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));
    } finally {
      mockTimer.uninstall();
    }
  });

  test('ClickWithoutBubbleOpenResumesCollapse', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(icon.hasAttribute('should-show-label'));
      icon.state = {
        ...icon.state,
        shouldRunAnimation: true,
        explanatoryString: 'Blocked',
      };
      await icon.updateComplete;
      mockTimer.tick(0);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      // Simulate a full click sequence. Crucially, because this is an isolated
      // WebUI test, the C++ backend is mocked out and will NOT return a
      // `trackedHighlighted = true` property. We are effectively testing the
      // fallback case where C++ fails/refuses to open a bubble. We want to
      // ensure the JS doesn't freeze the chip open forever waiting for it.
      icon.$.chip.dispatchEvent(new PointerEvent('pointerdown', {button: 0}));
      icon.$.chip.dispatchEvent(new PointerEvent('pointerup', {button: 0}));
      icon.$.chip.dispatchEvent(new PointerEvent('click', {button: 0}));
      assertEquals(1, handler.getCallCount('showContentSettingsBubble'));

      // The chip should still be expanded and should collapse after
      // COLLAPSE_HOLD_DURATION_MS.
      assertTrue(icon.hasAttribute('should-show-label'));
      mockTimer.tick(COLLAPSE_HOLD_DURATION_MS - 1);
      await icon.updateComplete;
      assertTrue(icon.hasAttribute('should-show-label'));

      mockTimer.tick(1);
      await icon.updateComplete;
      assertFalse(icon.hasAttribute('should-show-label'));
    } finally {
      mockTimer.uninstall();
    }
  });

  test('AnimationWithMultipleIcons', async () => {
    const container = document.createElement('content-settings-icons');
    document.body.appendChild(container);

    const cookiesState = {
      type: ContentSettingImageType.kCookies,
      isBlocked: true,
      tooltip: 'Cookies',
      accessibilityString: 'Cookies',
      shouldRunAnimation: false,
      explanatoryString: '',
      identifier: {
        nativeIdentifier: '',
        secondaryIdentifier: '',
      },
    };
    const popupsState = {
      type: ContentSettingImageType.kPopups,
      isBlocked: true,
      tooltip: 'Popups',
      accessibilityString: 'Popups',
      shouldRunAnimation: true,
      explanatoryString: 'Popups blocked',
      identifier: {
        nativeIdentifier: '',
        secondaryIdentifier: '',
      },
    };

    // Use order [Popups, Cookies] to test element reuse if Popups is removed.
    container.contentSettingImageStates = [popupsState, cookiesState];
    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();

    let icons = container.shadowRoot.querySelectorAll('content-setting-icon');
    assertEquals(2, icons.length);
    assertTrue(icons[0]!.hasAttribute('should-show-label'));
    assertTrue(icons[0]!.$.chip.hasAttribute('has-label'));
    const isReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    if (!isReducedMotion) {
      assertGT(icons[0]!.$.label.getAnimations().length, 0);
    }
    assertFalse(icons[1]!.hasAttribute('should-show-label'));
    assertFalse(icons[1]!.$.chip.hasAttribute('has-label'));
    assertEquals(0, icons[1]!.$.label.getAnimations().length);

    // Immediately remove the popups icon.
    container.contentSettingImageStates = [cookiesState];
    await microtasksFinished();
    // Yield to the event loop so macrotasks (like timers) can run.
    await new Promise(resolve => setTimeout(resolve, 0));
    await microtasksFinished();

    icons = container.shadowRoot.querySelectorAll('content-setting-icon');
    assertEquals(1, icons.length);
    assertEquals(ContentSettingImageType.kCookies, icons[0]!.state.type);
    assertFalse(icons[0]!.hasAttribute('should-show-label'));
    assertFalse(icons[0]!.$.chip.hasAttribute('has-label'));
    assertEquals(0, icons[0]!.$.label.getAnimations().length);
  });

  test('RightClick', () => {
    const button = icon.$.chip;

    // contextmenu should prevent default and NOT open the bubble.
    const contextMenuEvent =
        new PointerEvent('contextmenu', {cancelable: true});
    button.dispatchEvent(contextMenuEvent);
    assertTrue(contextMenuEvent.defaultPrevented);
    assertEquals(0, handler.getCallCount('showContentSettingsBubble'));

    // auxclick should open the bubble.
    button.dispatchEvent(new PointerEvent('auxclick', {button: 2}));
    assertEquals(1, handler.getCallCount('showContentSettingsBubble'));
  });

  test('Start tracking when identifier is set', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();

    assertEquals(1, startTrackingCalls.length);
    assertEquals(icon.$.chip, startTrackingCalls[0]![0]);
    assertEquals('test-id', startTrackingCalls[0]![1]);
    assertEquals(0, stopTrackingCalls.length);
  });

  test('Stop tracking when identifier is cleared', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: '',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();

    assertEquals(1, startTrackingCalls.length);
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(icon.$.chip, stopTrackingCalls[0]!);
  });

  test('Stop tracking and start tracking when identifier changes', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id-1',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id-2',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();

    assertEquals(1, stopTrackingCalls.length);
    assertEquals(icon.$.chip, stopTrackingCalls[0]!);
    assertEquals(2, startTrackingCalls.length);
    assertEquals(icon.$.chip, startTrackingCalls[1]![0]);
    assertEquals('test-id-2', startTrackingCalls[1]![1]);
  });

  test('Stop tracking on disconnected', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    icon.remove();
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(icon.$.chip, stopTrackingCalls[0]!);
  });
});
