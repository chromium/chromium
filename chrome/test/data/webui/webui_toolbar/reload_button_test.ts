// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ContextMenuType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';
import type {ReloadButtonElement} from 'chrome://webui-toolbar.top-chrome/reload_button.js';


class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['showContextMenu']);
  }

  showContextMenu(
      type: ContextMenuType, rect: DOMRect, source: MenuSourceType,
      showMenuToken: number|null = null) {
    this.methodCalled('showContextMenu', [type, rect, source, showMenuToken]);
  }
}

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
  let originalBeginElement: any;

  suiteSetup(function() {
    originalSetPointerCapture = HTMLElement.prototype.setPointerCapture;
    originalReleasePointerCapture = HTMLElement.prototype.releasePointerCapture;
    HTMLElement.prototype.setPointerCapture = () => {};
    HTMLElement.prototype.releasePointerCapture = () => {};

    // Prevent real SVG animations from playing to avoid timing flakes
    originalBeginElement = SVGAnimationElement.prototype.beginElement;
    SVGAnimationElement.prototype.beginElement = () => {};
  });

  suiteTeardown(function() {
    HTMLElement.prototype.setPointerCapture = originalSetPointerCapture;
    HTMLElement.prototype.releasePointerCapture = originalReleasePointerCapture;
    SVGAnimationElement.prototype.beginElement = originalBeginElement;
  });

  setup(async function() {
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

  function clickButton() {
    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    const rect = button.getBoundingClientRect();
    const clientX = rect.left + rect.width / 2;
    const clientY = rect.top + rect.height / 2;
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));
    button.dispatchEvent(new PointerEvent('pointerup', {
      button: 0,
      pointerType: 'mouse',
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));
  }

  // Tests that the standard cr-icon-button ink drop ripple is suppressed
  // when Glow Up is enabled, since Glow Up defines its own interactive
  // hover/click animations.
  test('GlowUp Disables Ripple', async function() {
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
    reloadButton.glowUpEnabled = true;
    await reloadButton.updateComplete;

    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    // Default fallback icon when not animating.
    assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));

    clickButton();
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
    clickButton();
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
    reloadButton.glowUpEnabled = true;
    await reloadButton.updateComplete;

    const button = reloadButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('webui-toolbar:refresh', button.getAttribute('iron-icon'));

    clickButton();
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
});
