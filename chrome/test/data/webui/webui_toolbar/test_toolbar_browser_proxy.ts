// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {DragEventSource} from 'chrome://resources/mojo/ui/base/dragdrop/mojom/drag_drop_types.mojom-webui.js';
import type {MenuSourceType} from 'chrome://resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {PointF, RectF} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {INVALID_FOCUS_REQUEST_HANDLE, INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE, INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {AdjustOmniboxTextForCopyResult, BrowserProxy, ContentSettingImageType, ContextMenuType, FocusRequestHandle, FocusRequestListener, InitialState, LhsChipIdentifier, NavigationControlsStateListener, NavigationControlsStateListenerHandle, OmniboxAction, PageActionId, PageActionTrigger, PinnedToolbarAction, ShowSplitTabsContextMenuHandle, ShowSplitTabsContextMenuListener, ToolbarUIServiceInterface} from 'chrome://webui-toolbar.top-chrome/app.js';

export class TestToolbarUiHandler extends TestBrowserProxy implements
    ToolbarUIServiceInterface {
  constructor() {
    super([
      'adjustOmniboxTextForCopy',
      'bind',
      'executeExtensionAction',
      'invokePinnedToolbarAction',
      'moveExtensionAction',
      'moveExtensionActionBy',
      'movePinnedToolbarAction',
      'movePinnedToolbarActionBy',
      'onAppMenuFocusChanged',
      'onContentSettingImageAnimationEnded',
      'onContentSettingImagePointerDown',
      'onExtensionActionPointerDown',
      'onHomeButtonDropFile',
      'onHomeButtonDropUrl',
      'onLhsChipClicked',
      'onLhsChipCollapseAnimationEnded',
      'onLhsChipDrag',
      'onLhsChipExpandAnimationEnded',
      'onLhsChipMousePressed',
      'onLhsChipPointerEntered',
      'onLhsChipPointerExited',
      'onLocationBarFocusWithinChanged',
      'onMediaButtonClicked',
      'onMediaButtonMousePressed',
      'onOmniboxAction',
      'onPageActionChipShowingChanged',
      'onPageActionClick',
      'onPageActionPointerDown',
      'onPageInitialized',
      'onPerformanceInterventionButtonClicked',
      'onPerformanceInterventionButtonMousePressed',
      'onToolbarDropFile',
      'setAvatarButtonFocused',
      'setAvatarButtonHovered',
      'setAvatarButtonIphPromoShowing',
      'showAvatarMenu',
      'showContentSettingsBubble',
      'showContextMenu',
      'showExtensionContextMenu',
      'showOverflowMenu',
    ]);
  }

  bind(): Promise<InitialState> {
    this.methodCalled('bind');
    return new Promise<never>(() => {});
  }

  showContextMenu(
      menuType: ContextMenuType, boundsInCssPixels: RectF,
      source: MenuSourceType, showMenuToken: number|null = null) {
    this.methodCalled(
        'showContextMenu',
        [menuType, boundsInCssPixels, source, showMenuToken]);
  }

  showOverflowMenu() {
    this.methodCalled('showOverflowMenu');
    return Promise.resolve({result: {}});
  }

  onOmniboxAction(action: OmniboxAction) {
    this.methodCalled('onOmniboxAction', action);
    return Promise.resolve({result: {}});
  }

  onPageInitialized() {
    this.methodCalled('onPageInitialized');
  }

  onContentSettingImagePointerDown(type: ContentSettingImageType) {
    this.methodCalled('onContentSettingImagePointerDown', type);
  }

  showContentSettingsBubble(
      type: ContentSettingImageType, isPointerInteraction: boolean) {
    this.methodCalled(
        'showContentSettingsBubble', [type, isPointerInteraction]);
    return Promise.resolve({result: {}});
  }

  onContentSettingImageAnimationEnded(type: ContentSettingImageType) {
    this.methodCalled('onContentSettingImageAnimationEnded', type);
  }

  onPageActionPointerDown(actionId: PageActionId) {
    this.methodCalled('onPageActionPointerDown', actionId);
  }

  onPageActionClick(actionId: PageActionId, trigger: PageActionTrigger) {
    this.methodCalled('onPageActionClick', [actionId, trigger]);
    return Promise.resolve({result: {}});
  }

  onPageActionChipShowingChanged(actionId: PageActionId) {
    this.methodCalled('onPageActionChipShowingChanged', actionId);
    return Promise.resolve({result: {}});
  }

  invokePinnedToolbarAction(actionId: PinnedToolbarAction) {
    this.methodCalled('invokePinnedToolbarAction', actionId);
  }

  movePinnedToolbarAction(actionId: PinnedToolbarAction, targetIndex: number) {
    this.methodCalled('movePinnedToolbarAction', [actionId, targetIndex]);
  }

  movePinnedToolbarActionBy(actionId: PinnedToolbarAction, delta: number) {
    this.methodCalled('movePinnedToolbarActionBy', [actionId, delta]);
  }

  moveExtensionAction(extensionId: string, targetIndex: number) {
    this.methodCalled('moveExtensionAction', [extensionId, targetIndex]);
  }

  moveExtensionActionBy(extensionId: string, delta: number) {
    this.methodCalled('moveExtensionActionBy', [extensionId, delta]);
  }

  onLhsChipMousePressed(identifier: LhsChipIdentifier, isMiddleClick: boolean) {
    this.methodCalled('onLhsChipMousePressed', [identifier, isMiddleClick]);
  }

  onLhsChipClicked(identifier: LhsChipIdentifier, isMouseInteraction: boolean) {
    this.methodCalled('onLhsChipClicked', [identifier, isMouseInteraction]);
  }

  onLhsChipPointerEntered(identifier: LhsChipIdentifier) {
    this.methodCalled('onLhsChipPointerEntered', identifier);
  }

  onLhsChipPointerExited(identifier: LhsChipIdentifier) {
    this.methodCalled('onLhsChipPointerExited', identifier);
  }

  onLhsChipExpandAnimationEnded(identifier: LhsChipIdentifier) {
    this.methodCalled('onLhsChipExpandAnimationEnded', identifier);
  }

  onLhsChipCollapseAnimationEnded(identifier: LhsChipIdentifier) {
    this.methodCalled('onLhsChipCollapseAnimationEnded', identifier);
  }

  onLhsChipDrag(identifier: LhsChipIdentifier, source: DragEventSource) {
    this.methodCalled('onLhsChipDrag', [identifier, source]);
  }

  onHomeButtonDropUrl(url: Url) {
    this.methodCalled('onHomeButtonDropUrl', url);
  }

  onHomeButtonDropFile(dropPosition: PointF) {
    this.methodCalled('onHomeButtonDropFile', dropPosition);
  }

  onToolbarDropFile(dropPosition: PointF) {
    this.methodCalled('onToolbarDropFile', dropPosition);
  }

  showAvatarMenu() {
    this.methodCalled('showAvatarMenu');
    return Promise.resolve({result: {}});
  }

  setAvatarButtonHovered(hovered: boolean) {
    this.methodCalled('setAvatarButtonHovered', hovered);
    return Promise.resolve({result: {}});
  }

  setAvatarButtonFocused(focused: boolean) {
    this.methodCalled('setAvatarButtonFocused', focused);
    return Promise.resolve({result: {}});
  }

  setAvatarButtonIphPromoShowing(showing: boolean) {
    this.methodCalled('setAvatarButtonIphPromoShowing', showing);
    return Promise.resolve({result: {}});
  }

  onAppMenuFocusChanged(focused: boolean) {
    this.methodCalled('onAppMenuFocusChanged', focused);
  }

  executeExtensionAction(extensionId: string, isPointerInteraction: boolean) {
    this.methodCalled(
        'executeExtensionAction', [extensionId, isPointerInteraction]);
  }

  onExtensionActionPointerDown(extensionId: string) {
    this.methodCalled('onExtensionActionPointerDown', extensionId);
  }

  showExtensionContextMenu(extensionId: string, source: MenuSourceType) {
    this.methodCalled('showExtensionContextMenu', [extensionId, source]);
  }

  onPerformanceInterventionButtonClicked(isMouseInteraction: boolean) {
    this.methodCalled(
        'onPerformanceInterventionButtonClicked', isMouseInteraction);
  }

  onPerformanceInterventionButtonMousePressed() {
    this.methodCalled('onPerformanceInterventionButtonMousePressed');
  }

  onMediaButtonClicked(isMouseInteraction: boolean) {
    this.methodCalled('onMediaButtonClicked', isMouseInteraction);
  }

  onMediaButtonMousePressed() {
    this.methodCalled('onMediaButtonMousePressed');
  }

  onLocationBarFocusWithinChanged(focusInside: boolean) {
    this.methodCalled('onLocationBarFocusWithinChanged', focusInside);
  }

  adjustOmniboxTextForCopy(text: String16, selectionStart: number):
      Promise<AdjustOmniboxTextForCopyResult> {
    this.methodCalled('adjustOmniboxTextForCopy', [text, selectionStart]);
    return Promise.resolve({
      adjustedText: text,
      adjustedUrl: null,
      pageTitle: null,
    });
  }
}

export class TestToolbarBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;
  browserControlsHandler: any = null;

  constructor() {
    super([
      'addFocusRequestListener',
      'addNavigationStateListener',
      'addShowSplitTabsContextMenuListener',
      'recordInHistogram',
      'removeFocusRequestListener',
      'removeNavigationStateListener',
      'removeShowSplitTabsContextMenuListener',
    ]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
  }

  recordInHistogram(histogramName: string, value: number, maxValue: number) {
    this.methodCalled('recordInHistogram', [histogramName, value, maxValue]);
  }

  addNavigationStateListener(_listener: NavigationControlsStateListener):
      NavigationControlsStateListenerHandle {
    return INVALID_NAVIGATION_CONTROLS_STATE_LISTENER_HANDLE;
  }

  removeNavigationStateListener(
      _handle: NavigationControlsStateListenerHandle) {}

  addFocusRequestListener(_listener: FocusRequestListener): FocusRequestHandle {
    return INVALID_FOCUS_REQUEST_HANDLE;
  }

  removeFocusRequestListener(_handle: FocusRequestHandle) {}

  addShowSplitTabsContextMenuListener(
      _listener: ShowSplitTabsContextMenuListener):
      ShowSplitTabsContextMenuHandle {
    return INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE;
  }

  removeShowSplitTabsContextMenuListener(
      _handle: ShowSplitTabsContextMenuHandle) {}

  onChipClicked(id: LhsChipIdentifier, isPointer: boolean) {
    this.toolbarUIHandler.onLhsChipClicked(id, isPointer);
  }

  onChipPointerEntered(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipPointerEntered(id);
  }

  onChipPointerExited(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipPointerExited(id);
  }

  onChipMousePressed(id: LhsChipIdentifier, isMiddleClick: boolean = false) {
    this.toolbarUIHandler.onLhsChipMousePressed(id, isMiddleClick);
  }

  onChipExpandAnimationEnded(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipExpandAnimationEnded(id);
  }

  onChipCollapseAnimationEnded(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipCollapseAnimationEnded(id);
  }

  showContextMenu(
      menuType: ContextMenuType, boundsInCssPixels: RectF,
      source: MenuSourceType, showMenuToken: number|null = null) {
    this.toolbarUIHandler.showContextMenu(
        menuType, boundsInCssPixels, source, showMenuToken);
  }
}
