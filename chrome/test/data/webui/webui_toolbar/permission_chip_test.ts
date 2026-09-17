// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, LhsChipIdentifier, PermissionAction, PermissionChipTheme, PermissionPromptStyle} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {PermissionChipElement, PermissionChipState} from 'chrome://webui-toolbar.top-chrome/app.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';
import type {TestToolbarUiHandler} from './test_toolbar_browser_proxy.js';

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
      iconName: 'kVideocamChromeRefreshOldIcon',
      theme: PermissionChipTheme.kNormalVisibility,
      promptStyle: PermissionPromptStyle.kChip,
      userDecision: PermissionAction.kGranted,
      shouldShowBlockedIcon: false,
      message: 'Camera',
      stateToken: 1,
    };
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    toolbarUiHandler = browserProxy.toolbarUIHandler;
    BrowserProxyImpl.setInstance(browserProxy);

    chip = document.createElement('permission-chip');
    chip.id = 'request-chip';
    chip.delegate = browserProxy;
    document.body.appendChild(chip);
  });

  test('Render invisible state', async function() {
    const state = createBaseState();
    state.isVisible = false;
    chip.chipState = state;
    await microtasksFinished();

    const chipEl = chip.shadowRoot.querySelector<HTMLElement>('#chip');
    assertTrue(!!chipEl);
    const style = window.getComputedStyle(chipEl);
    assertEquals('hidden', style.visibility);
    assertEquals('0', style.opacity);
  });

  test('Render visible state', async function() {
    chip.setAttribute('visible', '');
    chip.chipState = createBaseState();
    await microtasksFinished();

    const chipEl = chip.shadowRoot.querySelector<HTMLElement>('#chip');
    assertTrue(!!chipEl);
    const style = window.getComputedStyle(chipEl);
    assertEquals('visible', style.visibility);
    assertFalse(chipEl.hasAttribute('collapsed'));

    const iconEl = chip.shadowRoot.querySelector<CrIconElement>('#icon');
    assertTrue(!!iconEl);
    assertEquals('webui-toolbar-shared:videocam', iconEl.icon);

    const messageEl = chip.shadowRoot.querySelector<HTMLElement>('#message');
    assertTrue(!!messageEl);
    assertEquals('Camera', messageEl.textContent);

    // Verify it is a button for accessibility
    assertEquals('BUTTON', chipEl.tagName);
    assertEquals('Camera', chipEl.getAttribute('aria-label'));
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
        toolbarUiHandler.getArgs('onLhsChipMousePressed')[0][0]);

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
    assertEquals(1, toolbarUiHandler.getArgs('onLhsChipClicked')[0][2]);

    // Mouse click
    chipEl.dispatchEvent(new PointerEvent('click', {pointerType: 'mouse'}));
    assertEquals(2, toolbarUiHandler.getCallCount('onLhsChipClicked'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipClicked')[1][0]);
    assertTrue(toolbarUiHandler.getArgs('onLhsChipClicked')[1][1]);
    assertEquals(1, toolbarUiHandler.getArgs('onLhsChipClicked')[1][2]);

    // Click with non-zero stateToken forwards the token correctly.
    const stateWithToken = createBaseState();
    stateWithToken.stateToken = 42;
    chip.chipState = stateWithToken;
    await microtasksFinished();

    chipEl.click();
    assertEquals(3, toolbarUiHandler.getCallCount('onLhsChipClicked'));
    assertEquals(
        LhsChipIdentifier.kPermissionRequest,
        toolbarUiHandler.getArgs('onLhsChipClicked')[2][0]);
    assertEquals(42, toolbarUiHandler.getArgs('onLhsChipClicked')[2][2]);
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
