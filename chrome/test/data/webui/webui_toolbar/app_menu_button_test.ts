// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {AppMenuIconType, AppMenuSeverity, BrowserProxyImpl, ContextMenuType, FocusRequestTarget} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {AppMenuButtonElement, BrowserProxy, FocusRequestListener} from 'chrome://webui-toolbar.top-chrome/app.js';

import {TestToolbarUiHandler} from './test_toolbar_browser_proxy.js';

class MockBrowserProxy extends TestBrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;
  private focusRequestListener_: FocusRequestListener|null = null;

  constructor(toolbarUiHandler: TestToolbarUiHandler) {
    super(['addFocusRequestListener', 'removeFocusRequestListener']);
    this.toolbarUIHandler = toolbarUiHandler;
  }

  addFocusRequestListener(listener: FocusRequestListener): number {
    this.methodCalled('addFocusRequestListener', listener);
    this.focusRequestListener_ = listener;
    return 1;
  }

  removeFocusRequestListener(handle: number) {
    this.methodCalled('removeFocusRequestListener', handle);
    this.focusRequestListener_ = null;
  }

  triggerFocusRequest(target: FocusRequestTarget) {
    if (this.focusRequestListener_) {
      this.focusRequestListener_(target);
    }
  }
}

suite('AppMenuButtonTest', function() {
  let appMenuButton: AppMenuButtonElement;
  let toolbarUiHandler: TestToolbarUiHandler;
  let browserProxy: MockBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbarUiHandler = new TestToolbarUiHandler();
    browserProxy = new MockBrowserProxy(toolbarUiHandler);
    BrowserProxyImpl.setInstance(browserProxy as unknown as BrowserProxy);
    loadTimeData.overrideValues({
      enableGlowUp: false,
    });

    document.documentElement.style.setProperty(
        '--toolbar-interior-margin-end', '6px');
    document.documentElement.style.setProperty(
        '--toolbar-button-refresh-expanded-margin', '5px');
    document.documentElement.style.setProperty(
        '--toolbar-icon-default-margin', '2px');

    appMenuButton = document.createElement('app-menu-button');
    document.body.appendChild(appMenuButton);
    await microtasksFinished();
  });

  teardown(function() {
    document.documentElement.style.removeProperty(
        '--toolbar-interior-margin-end');
    document.documentElement.style.removeProperty(
        '--toolbar-button-refresh-expanded-margin');
    document.documentElement.style.removeProperty(
        '--toolbar-icon-default-margin');
  });

  test('Mouse Down Triggers Menu', function() {
    const button = appMenuButton.$.button;

    // Simulate mouse pointerdown (detail: 1 is required to be treated as mouse
    // click)
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'mouse',
      detail: 1,
    }));

    assertEquals(1, toolbarUiHandler.getCallCount('showContextMenu'));
    const args = toolbarUiHandler.getArgs('showContextMenu')[0];
    assertEquals(ContextMenuType.kAppMenu, args[0]);
    assertEquals(MenuSourceType.kMouse, args[2]);
  });

  test('Touch Down Does Not Trigger Menu', function() {
    const button = appMenuButton.$.button;

    // Simulate touch pointerdown
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerType: 'touch',
    }));

    assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
  });

  test('Touch Click Triggers Menu', function() {
    const button = appMenuButton.$.button;

    // Simulate touch click (detail > 0, pointerType: touch)
    button.dispatchEvent(new PointerEvent('click', {
      detail: 1,
      pointerType: 'touch',
    }));

    assertEquals(1, toolbarUiHandler.getCallCount('showContextMenu'));
    const args = toolbarUiHandler.getArgs('showContextMenu')[0];
    assertEquals(ContextMenuType.kAppMenu, args[0]);
    assertEquals(MenuSourceType.kTouch, args[2]);
  });

  test('Keyboard Click Triggers Menu', function() {
    const button = appMenuButton.$.button;

    // Simulate keyboard click (detail == 0)
    button.dispatchEvent(new PointerEvent('click', {
      detail: 0,
    }));

    assertEquals(1, toolbarUiHandler.getCallCount('showContextMenu'));
    const args = toolbarUiHandler.getArgs('showContextMenu')[0];
    assertEquals(ContextMenuType.kAppMenu, args[0]);
    assertEquals(MenuSourceType.kKeyboard, args[2]);
  });

  test('Mouse Click Ignored (Handled on Down)', function() {
    const button = appMenuButton.$.button;

    // Simulate mouse click (detail > 0, pointerType: mouse)
    button.dispatchEvent(new PointerEvent('click', {
      detail: 1,
      pointerType: 'mouse',
    }));

    assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
  });

  test('Non-Left Clicks Ignored on Down', function() {
    const button = appMenuButton.$.button;

    // Simulate middle click pointerdown
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 1,
      pointerType: 'mouse',
    }));

    // Simulate right click pointerdown
    button.dispatchEvent(new PointerEvent('pointerdown', {
      button: 2,
      pointerType: 'mouse',
    }));

    assertEquals(0, toolbarUiHandler.getCallCount('showContextMenu'));
  });

  test('Attribute Bindings', async function() {
    const button = appMenuButton.$.button;

    // 1. Verify Default State
    const innerButton = button.$.button;
    assertEquals('', button.ariaLabel);
    assertEquals('', button.tooltip);
    assertEquals('menu', button.ariaHasPopup);
    assertEquals('false', button.ariaExpanded);
    assertEquals('false', innerButton.getAttribute('aria-expanded'));
    assertFalse(button.hasAttribute('is-menu-open'));
    assertFalse(button.hasAttribute('has-label'));
    assertFalse(appMenuButton.hasAttribute('has-label'));
    assertFalse(!!button.querySelector('span'));

    // 2. Set Non-Default State 1
    appMenuButton.state = {
      iconType: AppMenuIconType.kNone,
      severity: AppMenuSeverity.kNone,
      labelText: 'Menu',
      accessibilityText: 'App Menu accessibility',
      tooltip: 'App Menu tooltip',
      isContextMenuVisible: true,
      windowIsMaximizedOrFullscreen: false,
    };
    await microtasksFinished();

    assertEquals('App Menu accessibility', button.ariaLabel);
    assertEquals('App Menu tooltip', button.tooltip);
    assertEquals('true', button.ariaExpanded);
    assertEquals('true', innerButton.getAttribute('aria-expanded'));
    assertTrue(button.hasAttribute('is-menu-open'));
    assertTrue(button.hasAttribute('has-label'));
    assertTrue(appMenuButton.hasAttribute('has-label'));

    let labelSpan = button.querySelector('span');
    assertTrue(!!labelSpan);
    assertEquals('Menu', labelSpan.textContent);

    // 3. Set Non-Default State 2 (verify changes)
    appMenuButton.state = {
      ...appMenuButton.state,
      labelText: 'New Label',
      accessibilityText: 'New A11y',
      tooltip: 'New Tooltip',
      isContextMenuVisible: false,
    };
    await microtasksFinished();

    assertEquals('New A11y', button.ariaLabel);
    assertEquals('New Tooltip', button.tooltip);
    assertEquals('false', button.ariaExpanded);
    assertEquals('false', innerButton.getAttribute('aria-expanded'));
    assertFalse(button.hasAttribute('is-menu-open'));
    assertTrue(button.hasAttribute('has-label'));
    assertTrue(appMenuButton.hasAttribute('has-label'));

    labelSpan = button.querySelector('span');
    assertTrue(!!labelSpan);
    assertEquals('New Label', labelSpan.textContent);

    // 4. Clear label to verify has-label attribute and span are removed
    appMenuButton.state = {
      ...appMenuButton.state,
      labelText: null,
    };
    await microtasksFinished();
    assertFalse(button.hasAttribute('has-label'));
    assertFalse(appMenuButton.hasAttribute('has-label'));
    assertFalse(!!button.querySelector('span'));
  });

  test('Severity Highlight Class', async function() {
    const button = appMenuButton.$.button;

    // Default: no severity, no class
    assertFalse(button.classList.contains('has-severity'));

    // Set severity
    appMenuButton.state = {
      iconType: AppMenuIconType.kNone,
      severity: AppMenuSeverity.kLow,
      labelText: null,
      accessibilityText: '',
      tooltip: '',
      isContextMenuVisible: false,
      windowIsMaximizedOrFullscreen: false,
    };
    await microtasksFinished();
    assertTrue(button.classList.contains('has-severity'));
  });

  test('Focus Request', function() {
    browserProxy.triggerFocusRequest(FocusRequestTarget.kAppMenu);

    let activeEl = document.activeElement;
    assertEquals('APP-MENU-BUTTON', activeEl?.tagName);

    activeEl = activeEl?.shadowRoot?.activeElement ?? null;
    assertEquals('TOOLBAR-CHIP-BUTTON', activeEl?.tagName);

    activeEl = activeEl?.shadowRoot?.activeElement ?? null;
    assertEquals('BUTTON', activeEl?.tagName);
  });

  test('Focusin/Focusout Reporting', function() {
    const button = appMenuButton.$.button;

    // Focus the button
    button?.dispatchEvent(new FocusEvent('focusin'));
    assertEquals(1, toolbarUiHandler.getCallCount('onAppMenuFocusChanged'));
    assertArrayEquals(
        [true], toolbarUiHandler.getArgs('onAppMenuFocusChanged'));

    // Blur the button
    button?.dispatchEvent(new FocusEvent('focusout'));
    assertEquals(2, toolbarUiHandler.getCallCount('onAppMenuFocusChanged'));
    assertArrayEquals(
        [true, false], toolbarUiHandler.getArgs('onAppMenuFocusChanged'));
  });

  test('Window State Margin', async function() {
    // Default: not maximized/fullscreen, attribute not present
    assertFalse(
        appMenuButton.hasAttribute('window-is-maximized-or-fullscreen'));
    assertEquals(
        '',
        getComputedStyle(appMenuButton)
            .getPropertyValue('--toolbar-chip-trailing-margin')
            .trim());

    // Set window to maximized/fullscreen
    appMenuButton.state = {
      ...appMenuButton.state,
      windowIsMaximizedOrFullscreen: true,
    };
    await microtasksFinished();
    assertTrue(appMenuButton.hasAttribute('window-is-maximized-or-fullscreen'));
    assertEquals(
        '6px',
        getComputedStyle(appMenuButton)
            .getPropertyValue('--toolbar-chip-trailing-margin')
            .trim());
  });

  test('Anchor Highlight Does Not Pulse', async function() {
    const visualTarget =
        appMenuButton.$.button.shadowRoot.querySelector('.iph-visual-target')!;
    assertTrue(!!visualTarget);

    appMenuButton.classList.add('anchor-highlight');
    await microtasksFinished();

    assertEquals(
        'none',
        window.getComputedStyle(visualTarget, '::before').animationName);
    assertEquals(
        '1',
        getComputedStyle(appMenuButton.$.button)
            .getPropertyValue('--toolbar-chip-highlight-opacity')
            .trim());
  });

  test('Help Bubble Activates Pulse Animation', async function() {
    const visualTarget =
        appMenuButton.$.button.shadowRoot.querySelector('.iph-visual-target')!;
    assertTrue(!!visualTarget);

    appMenuButton.hasHelpBubble = true;
    await microtasksFinished();

    assertTrue(
        appMenuButton.$.button.classList.contains('help-anchor-highlight'));
    assertEquals(
        'pulse',
        window.getComputedStyle(visualTarget, '::before').animationName);
    assertEquals(
        '1', window.getComputedStyle(visualTarget, '::before').opacity);

    appMenuButton.hasHelpBubble = false;
    await microtasksFinished();

    assertFalse(
        appMenuButton.$.button.classList.contains('help-anchor-highlight'));
  });

  test('Expanded Trailing Margin', async function() {
    // Collapsed: default 6px margin and 0px leading margin
    assertFalse(appMenuButton.hasAttribute('has-label'));
    assertFalse(appMenuButton.$.button.hasAttribute('has-label'));
    assertEquals('6px', window.getComputedStyle(appMenuButton).marginInlineEnd);
    assertEquals(
        '0px', window.getComputedStyle(appMenuButton).marginInlineStart);

    // Expanded with label: 7px trailing margin (6px + 1px) and 3px leading margin
    appMenuButton.state = {
      ...appMenuButton.state,
      labelText: 'Update',
    };
    await microtasksFinished();

    assertTrue(appMenuButton.hasAttribute('has-label'));
    assertTrue(appMenuButton.$.button.hasAttribute('has-label'));
    assertEquals('7px', window.getComputedStyle(appMenuButton).marginInlineEnd);
    assertEquals(
        '3px', window.getComputedStyle(appMenuButton).marginInlineStart);

    // Maximized/fullscreen mode: margin-inline-end is 0 and trailing margin
    // variable is 7px
    appMenuButton.state = {
      ...appMenuButton.state,
      windowIsMaximizedOrFullscreen: true,
    };
    await microtasksFinished();

    assertEquals('0px', window.getComputedStyle(appMenuButton).marginInlineEnd);
    assertEquals(
        '7px',
        window.getComputedStyle(appMenuButton.$.button).paddingInlineEnd);
  });

  test('Glow Up Disabled by Default', async function() {
    assertFalse(appMenuButton.glowUpEnabled);
    assertFalse(appMenuButton.glowUpActive);
    assertFalse(appMenuButton.hasAttribute('glow-up-active'));

    const crIcon = appMenuButton.shadowRoot.querySelector('cr-icon')!;
    assertTrue(!!crIcon);
    assertEquals('webui-toolbar:more_vert', crIcon.getAttribute('icon'));

    // Open context menu
    appMenuButton.state = {
      ...appMenuButton.state,
      isContextMenuVisible: true,
    };
    await microtasksFinished();

    assertFalse(appMenuButton.glowUpActive);
    assertFalse(appMenuButton.hasAttribute('glow-up-active'));
    assertFalse(appMenuButton.isAnimating);
    assertEquals('webui-toolbar:more_vert', crIcon.getAttribute('icon'));

    // Close context menu
    appMenuButton.state = {
      ...appMenuButton.state,
      isContextMenuVisible: false,
    };
    await microtasksFinished();

    assertFalse(appMenuButton.glowUpActive);
    assertFalse(appMenuButton.hasAttribute('glow-up-active'));
    assertFalse(appMenuButton.isAnimating);
    assertEquals('webui-toolbar:more_vert', crIcon.getAttribute('icon'));
  });

  test('Glow Up Animation Lifecycle and Ripple Suppression', async function() {
    // Intercept window.setTimeout to capture and manually trigger animation
    // timer callbacks (matching the 250ms glow-up duration) without leaking
    // test-only hooks into production code.
    const originalSetTimeout = window.setTimeout;
    let timeoutCallback: (() => void)|null = null;
    let timeoutDelay = 0;
    (window as any).setTimeout = (cb: any, ms: number) => {
      if (ms === 250) {
        timeoutCallback = cb;
        timeoutDelay = ms;
        return -1;
      }
      return originalSetTimeout(cb, ms);
    };

    try {
      appMenuButton.glowUpEnabled = true;
      assertFalse(appMenuButton.glowUpActive);

      const crIcon = appMenuButton.shadowRoot.querySelector('cr-icon')!;
      assertTrue(!!crIcon);
      assertEquals('webui-toolbar:more_vert', crIcon.getAttribute('icon'));

      // 1. Trigger menu open
      appMenuButton.state = {
        ...appMenuButton.state,
        isContextMenuVisible: true,
      };
      await microtasksFinished();

      // Glow Up should be active, animating, and forward animation icon set
      assertTrue(appMenuButton.glowUpActive);
      assertTrue(appMenuButton.hasAttribute('glow-up-active'));
      assertTrue(appMenuButton.isAnimating);
      assertEquals(
          'webui-toolbar:app_menu_glow_up', crIcon.getAttribute('icon'));

      // Ripple should be suppressed (transparent)
      assertEquals(
          'transparent',
          getComputedStyle(appMenuButton.$.button)
              .getPropertyValue('--toolbar-chip-ink-drop-ripple-color')
              .trim());

      // 2. Advance timer to complete opening animation
      assertTrue(!!timeoutCallback);
      assertEquals(250, timeoutDelay);
      (timeoutCallback as any)();
      timeoutCallback = null;
      await microtasksFinished();

      // Opening animation is complete, but menu is still open:
      // glowUpActive remains true, isAnimating becomes false
      assertFalse(appMenuButton.isAnimating);
      assertTrue(appMenuButton.glowUpActive);
      assertTrue(appMenuButton.hasAttribute('glow-up-active'));
      assertEquals(
          'webui-toolbar:app_menu_glow_up', crIcon.getAttribute('icon'));
      assertEquals(
          'transparent',
          getComputedStyle(appMenuButton.$.button)
              .getPropertyValue('--toolbar-chip-ink-drop-ripple-color')
              .trim());

      // 3. Trigger menu close
      appMenuButton.state = {
        ...appMenuButton.state,
        isContextMenuVisible: false,
      };
      await microtasksFinished();

      // During closing animation, end icon is shown, still active and
      // animating
      assertTrue(appMenuButton.glowUpActive);
      assertTrue(appMenuButton.hasAttribute('glow-up-active'));
      assertTrue(appMenuButton.isAnimating);
      assertEquals(
          'webui-toolbar:app_menu_glow_up_end', crIcon.getAttribute('icon'));

      // 4. Advance timer to complete closing animation
      assertTrue(!!timeoutCallback);
      assertEquals(250, timeoutDelay);
      (timeoutCallback as any)();
      timeoutCallback = null;
      await microtasksFinished();
      await appMenuButton.updateComplete;

      // Closing animation is complete:
      // reverts to inactive, not animating, and static icon restored
      assertFalse(appMenuButton.isAnimating);
      assertFalse(appMenuButton.glowUpActive);
      assertFalse(appMenuButton.hasAttribute('glow-up-active'));
      assertEquals('webui-toolbar:more_vert', crIcon.getAttribute('icon'));
    } finally {
      (window as any).setTimeout = originalSetTimeout;
    }
  });

  test('Glow Up Disconnect Cleans Up Timers and State', async function() {
    // Intercept window.clearTimeout to verify that removing the element from
    // the DOM properly cancels any pending animation timers.
    const originalClearTimeout = window.clearTimeout;
    let clearTimeoutCalled = false;
    (window as any).clearTimeout = (id: any) => {
      clearTimeoutCalled = true;
      originalClearTimeout(id);
    };

    try {
      appMenuButton.glowUpEnabled = true;

      // Open menu to begin animation
      appMenuButton.state = {
        ...appMenuButton.state,
        isContextMenuVisible: true,
      };
      await microtasksFinished();
      assertTrue(appMenuButton.isAnimating);

      // Remove from DOM
      appMenuButton.remove();

      assertFalse(appMenuButton.isAnimating);
      assertTrue(clearTimeoutCalled);
    } finally {
      (window as any).clearTimeout = originalClearTimeout;
    }
  });
});
