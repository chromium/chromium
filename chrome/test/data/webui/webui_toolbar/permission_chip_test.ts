// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import type {DragEventSource} from 'chrome://resources/mojo/ui/base/dragdrop/mojom/drag_drop_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE, LhsChipIdentifier, PermissionAction, PermissionChipTheme, PermissionPromptStyle} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {PermissionChipElement, PermissionChipState} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserControlsServiceInterface} from 'chrome://webui-toolbar.top-chrome/browser_controls_api.mojom-webui.js';
import type {BrowserProxy} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';
import type {ToolbarUIServiceInterface} from 'chrome://webui-toolbar.top-chrome/toolbar_ui_api.mojom-webui.js';

class TestToolbarUiHandler extends TestBrowserProxy implements
    ToolbarUIServiceInterface {
  constructor() {
    super([
      'onLhsChipMousePressed',
      'onLhsChipClicked',
      'onLhsChipCollapseAnimationEnded',
      'onLhsChipExpandAnimationEnded',
      'onLhsChipPointerEntered',
      'onLhsChipPointerExited',
      'onLhsChipDrag',
    ]);
  }

  bind() {
    return new Promise<never>(() => {});
  }
  showContextMenu() {}
  onOmniboxAction() {
    return new Promise<never>(() => {});
  }

  onPageInitialized() {}
  showContentSettingsBubble() {
    return new Promise<never>(() => {});
  }
  invokePinnedToolbarAction() {}
  onHomeButtonDropUrl() {}
  onHomeButtonDropFile() {}
  onToolbarDropFile() {}
  showAvatarMenu() {
    return new Promise<never>(() => {});
  }

  onLhsChipMousePressed(id: LhsChipIdentifier) {
    this.methodCalled('onLhsChipMousePressed', id);
  }

  onLhsChipClicked(id: LhsChipIdentifier, isMouseInteraction: boolean) {
    this.methodCalled('onLhsChipClicked', [id, isMouseInteraction]);
  }

  onLhsChipCollapseAnimationEnded(id: LhsChipIdentifier) {
    this.methodCalled('onLhsChipCollapseAnimationEnded', id);
  }

  onLhsChipExpandAnimationEnded(id: LhsChipIdentifier) {
    this.methodCalled('onLhsChipExpandAnimationEnded', id);
  }

  onLhsChipPointerEntered(id: LhsChipIdentifier) {
    this.methodCalled('onLhsChipPointerEntered', id);
  }

  onLhsChipPointerExited(id: LhsChipIdentifier) {
    this.methodCalled('onLhsChipPointerExited', id);
  }

  onLhsChipDrag(id: LhsChipIdentifier, source: DragEventSource) {
    this.methodCalled('onLhsChipDrag', [id, source]);
  }
}

class TestBrowserControlsHandler extends TestBrowserProxy implements
    BrowserControlsServiceInterface {
  constructor() {
    super([]);
  }
  stopLoad() {
    return new Promise<never>(() => {});
  }
  reloadFromClick() {
    return new Promise<never>(() => {});
  }
  splitActiveTab() {
    return new Promise<never>(() => {});
  }
  back() {
    return new Promise<never>(() => {});
  }
  forward() {
    return new Promise<never>(() => {});
  }
  backButtonHovered() {
    return new Promise<never>(() => {});
  }
  navigateHome() {
    return new Promise<never>(() => {});
  }
  navigate() {
    return new Promise<never>(() => {});
  }
}

class TestToolbarBrowserProxy extends TestBrowserProxy implements BrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;
  browserControlsHandler: TestBrowserControlsHandler;

  constructor() {
    super([
      'recordInHistogram',
      'addNavigationStateListener',
      'removeNavigationStateListener',
    ]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
    this.browserControlsHandler = new TestBrowserControlsHandler();
  }

  recordInHistogram() {}
  addNavigationStateListener() {
    return INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE;
  }
  addFocusRequestListener() {
    return INVALID_FOCUS_REQUEST_HANDLE;
  }
  removeNavigationStateListener() {}
  removeFocusRequestListener() {}
}

suite('PermissionChipTest', function() {
  let chip: PermissionChipElement;
  let toolbarUiHandler: TestToolbarUiHandler;
  let browserProxy: TestToolbarBrowserProxy;

  function createBaseState(): PermissionChipState {
    return {
      isFullyCollapsed: false,
      accessibilityName: 'Camera',
      tooltip: 'Camera in use',
      isVisible: true,
      iconName: 'kVideocamChromeRefreshIcon',
      theme: PermissionChipTheme.kNormalVisibility,
      promptStyle: PermissionPromptStyle.kChip,
      userDecision: PermissionAction.kGranted,
      shouldShowBlockedIcon: false,
      message: 'Camera',
    };
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    toolbarUiHandler = browserProxy.toolbarUIHandler;
    BrowserProxyImpl.setInstance(browserProxy);

    chip = document.createElement('permission-chip');
    chip.id = 'request-chip';
    document.body.appendChild(chip);
  });

  test('Render invisible state', async function() {
    const state = createBaseState();
    state.isVisible = false;
    chip.chipState = state;
    await microtasksFinished();

    const chipEl = chip.shadowRoot.querySelector<HTMLElement>('#chip');
    assertFalse(!!chipEl);
  });

  test('Render visible state', async function() {
    chip.chipState = createBaseState();
    await microtasksFinished();

    const chipEl = chip.shadowRoot.querySelector<HTMLElement>('#chip');
    assertTrue(!!chipEl);
    assertFalse(chipEl.hasAttribute('collapsed'));

    const iconEl = chip.shadowRoot.querySelector<HTMLElement>('#icon');
    assertTrue(!!iconEl);
    assertTrue(iconEl.style.maskImage.includes('videocam_chrome_refresh.svg'));

    const messageEl = chip.shadowRoot.querySelector<HTMLElement>('#message');
    assertTrue(!!messageEl);
    assertEquals('Camera', messageEl.textContent);
  });

  test('Click events', async function() {
    chip.chipState = createBaseState();
    await microtasksFinished();

    const chipEl = chip.shadowRoot.querySelector<HTMLElement>('#chip');
    assertTrue(!!chipEl);

    // Left press (pointerdown)
    chipEl.dispatchEvent(new PointerEvent('pointerdown', {button: 0}));
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipMousePressed')[0]);

    // Right press should not trigger pressed
    chipEl.dispatchEvent(new PointerEvent('pointerdown', {button: 2}));
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));

    // Programmatic click (e.g. keyboard)
    chipEl.click();
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipClicked'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipClicked')[0][0]);
    assertFalse(toolbarUiHandler.getArgs('onLhsChipClicked')[0][1]);

    // Mouse click
    chipEl.dispatchEvent(new PointerEvent('click', {pointerType: 'mouse'}));
    assertEquals(2, toolbarUiHandler.getCallCount('onLhsChipClicked'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipClicked')[1][0]);
    assertTrue(toolbarUiHandler.getArgs('onLhsChipClicked')[1][1]);
  });

  test('Pointer hover events', async function() {
    chip.chipState = createBaseState();
    await microtasksFinished();

    const chipEl = chip.shadowRoot.querySelector<HTMLElement>('#chip');
    assertTrue(!!chipEl);

    chipEl.dispatchEvent(new PointerEvent('pointerenter'));
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipPointerEntered'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipPointerEntered')[0]);

    chipEl.dispatchEvent(new PointerEvent('pointerleave'));
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipPointerExited'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipPointerExited')[0]);
  });

  test('Theme colors', async function() {
    const state = createBaseState();

    // Test Activity Indicator
    state.theme = PermissionChipTheme.kActivityIndicator;
    chip.chipState = {...state};
    await microtasksFinished();

    let bgColor = chip.style.getPropertyValue('--chip-bg-color').trim();
    let fgColor = chip.style.getPropertyValue('--chip-fg-color').trim();
    assertEquals(
        'var(--color-omnibox-chip-in-use-activity-indicator-background)',
        bgColor);
    assertEquals(
        'var(--color-omnibox-chip-in-use-activity-indicator-foreground)',
        fgColor);

    // Test Blocked Activity Indicator
    state.theme = PermissionChipTheme.kBlockedActivityIndicator;
    chip.chipState = {...state};
    await microtasksFinished();

    bgColor = chip.style.getPropertyValue('--chip-bg-color').trim();
    fgColor = chip.style.getPropertyValue('--chip-fg-color').trim();
    assertEquals(
        'var(--color-omnibox-chip-blocked-activity-indicator-background)',
        bgColor);
    assertEquals(
        'var(--color-omnibox-chip-blocked-activity-indicator-foreground)',
        fgColor);

    // Test Normal Visibility with Granted
    state.theme = PermissionChipTheme.kNormalVisibility;
    state.userDecision = PermissionAction.kGranted;
    chip.chipState = {...state};
    await microtasksFinished();

    bgColor = chip.style.getPropertyValue('--chip-bg-color').trim();
    fgColor = chip.style.getPropertyValue('--chip-fg-color').trim();
    assertEquals('var(--color-omnibox-chip-background)', bgColor);
    assertEquals(
        'var(--color-omnibox-chip-foreground-normal-visibility)', fgColor);

    // Test Normal Visibility with Denied
    state.userDecision = PermissionAction.kDenied;
    chip.chipState = {...state};
    await microtasksFinished();

    bgColor = chip.style.getPropertyValue('--chip-bg-color').trim();
    fgColor = chip.style.getPropertyValue('--chip-fg-color').trim();
    assertEquals('var(--color-omnibox-chip-background)', bgColor);
    assertEquals(
        'var(--color-omnibox-chip-foreground-low-visibility)', fgColor);
  });
});
