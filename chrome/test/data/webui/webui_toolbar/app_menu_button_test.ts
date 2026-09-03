// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {AppMenuIconType, AppMenuSeverity, BrowserProxyImpl, ContextMenuType, FocusRequestTarget} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {AppMenuButtonElement} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy, FocusRequestListener} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';

class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['showContextMenu', 'onAppMenuFocusChanged']);
  }

  showContextMenu(
      type: ContextMenuType, rect: DOMRect, source: MenuSourceType,
      showMenuToken: number|null = null) {
    this.methodCalled('showContextMenu', [type, rect, source, showMenuToken]);
  }

  onAppMenuFocusChanged(focused: boolean) {
    this.methodCalled('onAppMenuFocusChanged', focused);
  }
}

class MockBrowserProxy extends TestBrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;
  private focusRequestListener_: FocusRequestListener|null = null;

  constructor(toolbarUiHandler: TestToolbarUiHandler) {
    super(['addFocusRequestListener', 'removeFocusRequestListener']);
    this.toolbarUIHandler = toolbarUiHandler;
  }

  addFocusRequestListener(listener: FocusRequestListener) {
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

    appMenuButton = document.createElement('app-menu-button');
    document.body.appendChild(appMenuButton);
    await microtasksFinished();
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
});
