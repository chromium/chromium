// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ContextMenuType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BackForwardButtonElement} from 'chrome://webui-toolbar.top-chrome/back_forward_button.js';
import type {BrowserProxy} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';

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
    super(['back', 'forward', 'backButtonHovered']);
  }

  back(flags: any) {
    this.methodCalled('back', flags);
  }

  forward(flags: any) {
    this.methodCalled('forward', flags);
  }

  backButtonHovered() {
    this.methodCalled('backButtonHovered');
  }
}

suite('BackForwardButtonTest', function() {
  let backForwardButton: BackForwardButtonElement;
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
    } as unknown as BrowserProxy);
    loadTimeData.overrideValues({
      roundedIconsEnabled: true,
    });

    backForwardButton = document.createElement('back-forward-button');
    backForwardButton.id = 'back';
    document.body.appendChild(backForwardButton);
    await microtasksFinished();
  });

  test('Window State Margin', async function() {
    // Explicitly set --toolbar-interior-margin-start so the test assumption
    // holds across platforms regardless of touch UI mode (where interior margin
    // defaults to 0px on ChromeOS Ash).
    backForwardButton.style.setProperty(
        '--toolbar-interior-margin-start', '6px');
    const buttonWrapper =
        backForwardButton.shadowRoot.querySelector('#buttonWrapper')!;

    // Default: not maximized/fullscreen, attribute not present.
    assertFalse(
        backForwardButton.hasAttribute('window-is-maximized-or-fullscreen'));
    const style1 = window.getComputedStyle(buttonWrapper);
    assertEquals('0px', style1.paddingLeft);
    assertEquals('6px', style1.marginLeft);

    // Set to maximized/fullscreen
    backForwardButton.windowIsMaximizedOrFullscreen = true;
    await microtasksFinished();

    assertTrue(
        backForwardButton.hasAttribute('window-is-maximized-or-fullscreen'));
    const style2 = window.getComputedStyle(buttonWrapper);
    assertEquals('6px', style2.paddingLeft);
    assertEquals('0px', style2.marginLeft);
  });

  test('Click Host Triggers Action and Ripple', async function() {
    // Enable the button so it can be clicked
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    backForwardButton.windowIsMaximizedOrFullscreen = true;
    await microtasksFinished();

    const buttonWrapper =
        backForwardButton.shadowRoot.querySelector('#buttonWrapper')!;
    const button =
        backForwardButton.shadowRoot.querySelector('cr-icon-button')!;
    const rect = backForwardButton.getBoundingClientRect();
    // Click in the leading margin (left 4px of the expanded width)
    const clientX = rect.left + 4;
    const clientY = rect.top + rect.height / 2;

    // Spy on the ripple methods to verify forwarding from wrapper to ripple.
    const ripple = button.getRipple();
    let downActionCalled = false;
    let upActionCalled = false;
    const originalUiDownAction = ripple.uiDownAction.bind(ripple);
    const originalUiUpAction = ripple.uiUpAction.bind(ripple);
    ripple.uiDownAction = (e?: PointerEvent) => {
      downActionCalled = true;
      originalUiDownAction(e);
    };
    ripple.uiUpAction = () => {
      upActionCalled = true;
      originalUiUpAction();
    };

    // Click on the buttonWrapper (representing the padding/margin area)
    buttonWrapper.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));
    assertTrue(button.hasRipple());
    assertTrue(downActionCalled);
    assertEquals(
        backForwardButton.glowUpEnabled ? 0 : 1,
        ripple.shadowRoot.querySelectorAll('.ripple').length);

    buttonWrapper.dispatchEvent(new PointerEvent('pointerup', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));
    assertTrue(upActionCalled);

    // Verify the back action was triggered
    assertEquals(1, browserControlsHandler.getCallCount('back'));
  });

  test('Manual Ripple Clears On Drag Release Over Button', async function() {
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    backForwardButton.windowIsMaximizedOrFullscreen = true;
    await microtasksFinished();

    const buttonWrapper =
        backForwardButton.shadowRoot.querySelector('#buttonWrapper')!;
    const button =
        backForwardButton.shadowRoot.querySelector('cr-icon-button')!;

    const ripple = button.getRipple();
    let upActionCalled = false;
    const originalUiUpAction = ripple.uiUpAction.bind(ripple);
    ripple.uiUpAction = () => {
      upActionCalled = true;
      originalUiUpAction();
    };

    // Simulate pointerdown on the outer wrapper padding space.
    buttonWrapper.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
      bubbles: true,
      composed: true,
    }));
    assertTrue(button.hasRipple());

    // Simulate pointer dragging over onto #button and releasing there.
    button.dispatchEvent(new PointerEvent('pointerup', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
      bubbles: true,
      composed: true,
    }));

    assertTrue(upActionCalled);
  });

  test(
      'Manual Ripple Avoids Redundant Up Actions On Leave And Release',
      async function() {
        backForwardButton.state = {
          enabled: true,
          shouldBeShown: true,
          isContextMenuVisible: false,
        };
        backForwardButton.direction = 'back';
        await microtasksFinished();

        const buttonWrapper =
            backForwardButton.shadowRoot.querySelector('#buttonWrapper')!;
        const button =
            backForwardButton.shadowRoot.querySelector('cr-icon-button')!;

        const ripple = button.getRipple();
        let upActionCallCount = 0;
        const originalUiUpAction = ripple.uiUpAction.bind(ripple);
        ripple.uiUpAction = () => {
          upActionCallCount++;
          originalUiUpAction();
        };

        // Simulate pointerdown on the outer wrapper padding space.
        buttonWrapper.dispatchEvent(new PointerEvent('pointerdown', {
          button: 0,
          pointerType: 'mouse',
          detail: 1,
          bubbles: true,
          composed: true,
        }));
        assertTrue(button.hasRipple());

        // Simulate pointer dragging outside (pointerleave) then pointerup.
        buttonWrapper.dispatchEvent(new PointerEvent('pointerleave', {
          button: 0,
          pointerType: 'mouse',
          detail: 1,
          bubbles: true,
          composed: true,
        }));
        buttonWrapper.dispatchEvent(new PointerEvent('pointerup', {
          button: 0,
          pointerType: 'mouse',
          detail: 1,
          bubbles: true,
          composed: true,
        }));

        assertEquals(1, upActionCallCount);
      });

  test('Click Button Directly Triggers Action', async function() {
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    await microtasksFinished();

    const button = backForwardButton.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);

    const rect = button.getBoundingClientRect();
    const clientX = rect.left + rect.width / 2;
    const clientY = rect.top + rect.height / 2;

    // Click on the internal button
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));
    button.dispatchEvent(new PointerEvent('pointerup', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));

    // Verify the back action was triggered
    assertEquals(1, browserControlsHandler.getCallCount('back'));
  });

  // Verify that non-back buttons (like Forward) render a standard flex
  // wrapper without Fitts' law edge margins or padding.
  test('Forward Button Wrapper Layout', async function() {
    const forwardButton = document.createElement('back-forward-button');
    forwardButton.id = 'forward';
    document.body.appendChild(forwardButton);
    await microtasksFinished();

    const buttonWrapper =
        forwardButton.shadowRoot.querySelector('#buttonWrapper')!;
    const style = window.getComputedStyle(buttonWrapper);
    assertEquals('flex', style.display);
    assertEquals('0px', style.paddingLeft);
  });

  test(
      'Disabled Button Does Not Trigger Action On Wrapper Click',
      async function() {
        backForwardButton.state = {
          enabled: false,
          shouldBeShown: true,
          isContextMenuVisible: false,
        };
        backForwardButton.direction = 'back';
        await microtasksFinished();

        const buttonWrapper =
            backForwardButton.shadowRoot.querySelector('#buttonWrapper')!;
        buttonWrapper.dispatchEvent(new PointerEvent('pointerdown', {
          button: 0,
          pointerType: 'mouse',
          detail: 1,
          clientX: 5,
          clientY: 5,
          bubbles: true,
          composed: true,
        }));
        buttonWrapper.dispatchEvent(new PointerEvent('pointerup', {
          button: 0,
          pointerType: 'mouse',
          detail: 1,
          clientX: 5,
          clientY: 5,
          bubbles: true,
          composed: true,
        }));

        assertEquals(0, browserControlsHandler.getCallCount('back'));
      });

  // Tests the Glow Up animation lifecycle for back/forward buttons:
  // 1. Initially shows the static fallback icon.
  // 2. On click, transitions to the animated "glow up" icon.
  // 3. Once the animation finishes (simulated by dispatching 'endEvent'),
  //    reverts back to the static fallback icon.
  test('GlowUp Animation Flow', async function() {
    backForwardButton.glowUpEnabled = true;
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    await backForwardButton.updateComplete;

    const button =
        backForwardButton.shadowRoot.querySelector('cr-icon-button')!;
    // Default fallback icon when not animating.
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));

    // Trigger click
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

    await browserControlsHandler.whenCalled('back');
    // We need to wait for:
    // 1. backForwardButton to update and set the iron-icon attribute on the
    // button.
    // 2. The button (cr-icon-button) to update its internal template.
    // 3. The cr-icon inside the button to update.
    // Awaiting crIcon.updateComplete is crucial because playAnimation_ is async
    // and awaits it internally. We must ensure playAnimation_ has resumed and
    // registered the 'endEvent' listener before we dispatch it in the test.
    await backForwardButton.updateComplete;
    await button.updateComplete;
    const crIcon = button.shadowRoot.querySelector('cr-icon')!;
    if (crIcon && 'updateComplete' in crIcon) {
      await (crIcon as any).updateComplete;
    }

    // Should be animating now.
    assertTrue((backForwardButton as any).animating_);
    assertEquals(
        'webui-toolbar:back_arrow_glow_up', button.getAttribute('iron-icon'));

    // Simulate animation end
    const animate =
        button.shadowRoot.querySelector('cr-icon')!.shadowRoot.querySelector(
            'animate[begin="indefinite"], animateTransform[begin="indefinite"]')!
        ;

    animate.dispatchEvent(new CustomEvent('endEvent'));
    await backForwardButton.updateComplete;

    // Should return to fallback icon.
    assertFalse((backForwardButton as any).animating_);
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));
  });

  test('GlowUp Animation Flow - Disabled for Touch UI', async function() {
    backForwardButton.glowUpEnabled = true;
    backForwardButton.touchUi = true;
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    await backForwardButton.updateComplete;

    const button =
        backForwardButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));

    // Trigger click (pointerup with mouse left click)
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

    await browserControlsHandler.whenCalled('back');
    await backForwardButton.updateComplete;
    await button.updateComplete;

    // Should NOT be animating.
    assertFalse((backForwardButton as any).animating_);
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));
  });

  test('GlowUp Animation Flow - Disabled for Non-Left Click', async function() {
    backForwardButton.glowUpEnabled = true;
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    await backForwardButton.updateComplete;

    const button =
        backForwardButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));

    // Trigger middle click (pointerup with button 1)
    const rect = button.getBoundingClientRect();
    const clientX = rect.left + rect.width / 2;
    const clientY = rect.top + rect.height / 2;
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 1,
      pointerType: 'mouse',
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));
    button.dispatchEvent(new PointerEvent('pointerup', {
      button: 1,
      pointerType: 'mouse',
      clientX: clientX,
      clientY: clientY,
      bubbles: true,
      composed: true,
    }));

    await browserControlsHandler.whenCalled('back');
    await backForwardButton.updateComplete;
    await button.updateComplete;

    // Should NOT be animating.
    assertFalse((backForwardButton as any).animating_);
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));
  });

  test('GlowUp Animation Flow - Disabled for Keyboard Click', async function() {
    backForwardButton.glowUpEnabled = true;
    backForwardButton.state = {
      enabled: true,
      shouldBeShown: true,
      isContextMenuVisible: false,
    };
    backForwardButton.direction = 'back';
    await backForwardButton.updateComplete;

    const button =
        backForwardButton.shadowRoot.querySelector('cr-icon-button')!;
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));

    // Trigger keyboard click (click event with detail 0)
    button.dispatchEvent(new MouseEvent('click', {
      detail: 0,
      bubbles: true,
      composed: true,
    }));

    await browserControlsHandler.whenCalled('back');
    await backForwardButton.updateComplete;
    await button.updateComplete;

    // Should NOT be animating.
    assertFalse((backForwardButton as any).animating_);
    assertEquals('webui-toolbar:arrow_back', button.getAttribute('iron-icon'));
  });
});
