// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {BrowserProxyImpl, ContextMenuType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BatterySaverButtonElement} from 'chrome://webui-toolbar.top-chrome/app.js';
import {INVALID_FOCUS_REQUEST_HANDLE, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE, INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';
import type {BrowserControlsServiceInterface} from 'chrome://webui-toolbar.top-chrome/shared/browser_controls_api.mojom-webui.js';
import type {ToolbarUIServiceInterface} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api.mojom-webui.js';

// TODO: Centralize the stubs used across tests in this directory.
class TestToolbarUiHandler extends TestBrowserProxy implements
    ToolbarUIServiceInterface {
  constructor() {
    super(['showContextMenu']);
  }

  bind() {
    return new Promise<never>(() => {});
  }
  showContextMenu(
      menuType: number, bounds: any, source: number,
      showMenuToken: number|null = null) {
    this.methodCalled(
        'showContextMenu', {menuType, bounds, source, showMenuToken});
  }
  showOverflowMenu() {
    return Promise.resolve({result: {}});
  }
  onOmniboxAction() {
    return new Promise<never>(() => {});
  }
  onPageInitialized() {}
  onContentSettingImagePointerDown() {}
  showContentSettingsBubble() {
    return new Promise<never>(() => {});
  }
  onContentSettingImageAnimationEnded() {}
  onPageActionPointerDown() {}
  onPageActionClick() {
    return new Promise<never>(() => {});
  }
  onPageActionChipShowingChanged() {
    return new Promise<never>(() => {});
  }
  invokePinnedToolbarAction() {}
  movePinnedToolbarAction() {}
  movePinnedToolbarActionBy() {}
  moveExtensionAction() {}
  moveExtensionActionBy() {}
  onHomeButtonDropUrl() {}
  onHomeButtonDropFile() {}
  onToolbarDropFile() {}
  showAvatarMenu() {
    return new Promise<never>(() => {});
  }
  setAvatarButtonHovered(_hovered: boolean) {
    return new Promise<never>(() => {});
  }
  setAvatarButtonFocused(_focused: boolean) {
    return new Promise<never>(() => {});
  }
  setAvatarButtonIphPromoShowing(_showing: boolean) {
    return new Promise<never>(() => {});
  }
  onAppMenuFocusChanged() {}
  executeExtensionAction() {}
  showExtensionContextMenu() {}
  onPerformanceInterventionButtonClicked() {}
  onPerformanceInterventionButtonMousePressed() {}
  onLocationBarFocusWithinChanged() {}
  onLhsChipMousePressed() {}
  onLhsChipClicked() {}
  onLhsChipCollapseAnimationEnded() {}
  onLhsChipExpandAnimationEnded() {}
  onLhsChipPointerEntered() {}
  onLhsChipPointerExited() {}
  onLhsChipDrag() {}
  adjustOmniboxTextForCopy() {
    return Promise.resolve({
      adjustedText: '',
      adjustedUrl: null,
      pageTitle: null,
    });
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
  navigateText() {
    return new Promise<never>(() => {});
  }
}

class TestBatterySaverBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;
  browserControlsHandler: TestBrowserControlsHandler;

  constructor() {
    super([]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
    this.browserControlsHandler = new TestBrowserControlsHandler();
  }

  // BrowserProxy
  recordInHistogram() {}
  addNavigationStateListener() {
    return INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE;
  }
  addFocusRequestListener() {
    return INVALID_FOCUS_REQUEST_HANDLE;
  }
  addShowSplitTabsContextMenuListener() {
    return INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE;
  }
  removeNavigationStateListener() {}
  removeFocusRequestListener() {}
  removeShowSplitTabsContextMenuListener() {}

  onChipClicked() {}
  onChipPointerEntered() {}
  onChipPointerExited() {}
  onChipMousePressed() {}
  onChipExpandAnimationEnded() {}
  onChipCollapseAnimationEnded() {}

  showContextMenu(
      menuType: number, bounds: any, source: number,
      showMenuToken: number|null = null) {
    this.methodCalled(
        'showContextMenu', {menuType, bounds, source, showMenuToken});
  }
}

suite('BatterySaverButton', function() {
  let button: BatterySaverButtonElement;
  let browserProxy: TestBatterySaverBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestBatterySaverBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy as BrowserProxy);

    button = document.createElement('battery-saver-button');
    document.body.appendChild(button);
  });

  test('ClickShowsBubble', async () => {
    assertEquals(
        0, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));

    assertTrue(!!button.shadowRoot, 'shadowRoot should not be null');
    const crIconButton = button.$.button;

    // Simulate click
    crIconButton.click();

    const args =
        await browserProxy.toolbarUIHandler.whenCalled('showContextMenu');
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
    assertEquals(ContextMenuType.kBatterySaver, args.menuType);
  });

  test('ShowsTooltip', () => {
    const crIconButton = button.$.button;
    // Tooltip and aria-label strings should be set
    assertEquals('Energy Saver is on', crIconButton.title);
    assertEquals('Energy Saver is on', crIconButton.getAttribute('aria-label'));
  });

  test('TabindexIsZero', () => {
    const crIconButton = button.$.button;
    assertEquals('0', crIconButton.getAttribute('tabindex'));
  });
});
