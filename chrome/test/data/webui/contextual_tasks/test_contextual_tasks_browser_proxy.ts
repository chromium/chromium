// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionPageCallbackRouter} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import type {ContextualTaskId, ContextualWindowId, ExtensionPageHandlerInterface, ExtensionPageRemote, PageHandlerInterface} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import type {ExtensionBrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ContextualTasksToolbarUIObserverCallbackRouter, PageCallbackRouter as ToolbarPageCallbackRouter} from 'chrome://contextual-tasks/contextual_tasks_toolbar.mojom-webui.js';
import type {ContextualTasksToolbarUIObserverRemote, ContextualTasksToolbarUIServiceInterface, InitialState, PageHandlerInterface as ToolbarPageHandlerInterface, PageRemote as ToolbarPageRemote} from 'chrome://contextual-tasks/contextual_tasks_toolbar.mojom-webui.js';
import type {ToolbarBrowserProxy} from 'chrome://contextual-tasks/contextual_tasks_toolbar_browser_proxy.js';
import type {PostMessageHandler} from 'chrome://contextual-tasks/post_message_handler.js';
import type {LhsChipIdentifier} from 'chrome://contextual-tasks/toolbar_ui_api_data_model.mojom-webui.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {HANDSHAKE_RESPONSE_BYTES} from './contextual_tasks_test_utils.js';

/**
 * Test version of the ContextualTasksPageHandler used to verify calls to the
 * browser from WebUI.
 */
export class TestContextualTasksPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private url_: Url;
  private isInTab_: boolean = true;
  private postMessageHandler_: PostMessageHandler|null = null;
  private isAiPageResult_: boolean = false;
  private isPendingErrorPageMap_: {[key: string]: boolean} = {};
  private isInZeroState_: boolean = false;
  private movedToNewThread_: boolean = false;


  constructor(url: string) {
    super([
      'setThreadUrl',
      'getCommonSearchParams',
      'getRecentTabs',
      'getSearchUrl',
      'getThreadUrl',
      'getUrlForTask',
      'isAiPage',
      'isPendingErrorPage',
      'isEmbeddedPageErrorDocument',
      'isShownInTab',
      'isZeroState',
      'moveActiveTabToNewThreadIfShared',
      'onboardingTooltipDismissed',
      'askGTooltipDismissed',
      'onContextMenuOpened',
      'onWebviewMessage',
      'openOnboardingHelpUi',
      'openAskGHelpUi',
      'setTaskId',
      'setThreadTitle',
      'submitQuery',
      'isSidePanelPinned',
      'notifySmartTabSharingTryItIphResult',
      'notifySmartTabSharingDefaultOnIphResult',
      'registerWindow',
      'onWindowClosed',
      'closeWindow',
    ]);

    this.url_ = url;
  }

  setPostMessageHandler(handler: PostMessageHandler) {
    this.postMessageHandler_ = handler;
  }

  setThreadUrl(url: string) {
    this.url_ = url;
  }

  getThreadUrl() {
    this.methodCalled('getThreadUrl');
    return Promise.resolve({url: this.url_ as unknown as Url});
  }

  setMovedToNewThread(moved: boolean) {
    this.movedToNewThread_ = moved;
  }

  moveActiveTabToNewThreadIfShared() {
    this.methodCalled('moveActiveTabToNewThreadIfShared');
    return Promise.resolve({moved: this.movedToNewThread_});
  }

  getUrlForTask(uuid: Uuid) {
    this.methodCalled('getUrlForTask', uuid);
    return Promise.resolve({url: this.url_ as unknown as Url});
  }

  setTaskId(uuid: Uuid) {
    this.methodCalled('setTaskId', uuid);
  }

  setThreadTitle(title: string) {
    this.methodCalled('setThreadTitle', title);
  }

  setIsShownInTab(isInTab: boolean) {
    this.isInTab_ = isInTab;
  }

  isShownInTab() {
    this.methodCalled('isShownInTab');
    return Promise.resolve({isInTab: this.isInTab_});
  }

  setIsZeroState(isZeroState: boolean) {
    this.isInZeroState_ = isZeroState;
  }

  isZeroState(url: Url) {
    this.methodCalled('isZeroState', url);
    return Promise.resolve({isZeroState: this.isInZeroState_});
  }

  setIsAiPage(isAiPage: boolean) {
    this.isAiPageResult_ = isAiPage;
  }

  isAiPage(url: Url) {
    this.methodCalled('isAiPage', url);
    return Promise.resolve({isAiPage: this.isAiPageResult_});
  }

  setIsPendingErrorPage(taskId: Uuid, isPendingErrorPage: boolean) {
    this.isPendingErrorPageMap_[taskId.value] = isPendingErrorPage;
  }

  isPendingErrorPage(taskId: Uuid) {
    this.methodCalled('isPendingErrorPage', taskId);
    const isPendingErrorPage =
        this.isPendingErrorPageMap_[taskId.value] ?? false;
    return Promise.resolve({isPendingErrorPage: isPendingErrorPage});
  }

  isEmbeddedPageErrorDocument() {
    this.methodCalled('isEmbeddedPageErrorDocument');
    return Promise.resolve({isErrorDocument: false});
  }

  openOnboardingHelpUi() {
    this.methodCalled('openOnboardingHelpUi');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  openAskGHelpUi() {
    this.methodCalled('openAskGHelpUi');
  }

  onboardingTooltipDismissed() {
    this.methodCalled('onboardingTooltipDismissed');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  askGTooltipDismissed() {
    this.methodCalled('askGTooltipDismissed');
  }

  // This name is generated by mojom, and the convention is to use OAuth, which
  // violates the linter check. To keep conventions, disable the linter here.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  getOAuthToken() {
    this.methodCalled('getOAuthToken');
    return Promise.resolve({oauthToken: 'fake_token'});
  }

  getRecentTabs() {
    this.methodCalled('getRecentTabs');
    return Promise.resolve({tabs: []});
  }

  getCommonSearchParams(isDarkMode: boolean, isSidePanel: boolean) {
    this.methodCalled('getCommonSearchParams', isDarkMode, isSidePanel);
    const params: {[key: string]: string} = {
      'hl': 'en-US',
      'cs': isDarkMode ? '1' : '0',
      'gsc': isSidePanel ? '2' : '',
    };
    return Promise.resolve({params});
  }

  getSearchUrl(query: string) {
    this.methodCalled('getSearchUrl', query);
    return Promise.resolve({url: {url: 'https://test.com/?q=' + query}});
  }

  onWebviewMessage(message: number[]) {
    this.methodCalled('onWebviewMessage', message);
    // Simulate the C++ side calling onHandshakeComplete on the Page remote
    // if the message is a handshake response.
    const messageUint8 = new Uint8Array(message);
    let isHandshakeResponse =
        messageUint8.length === HANDSHAKE_RESPONSE_BYTES.length;
    if (isHandshakeResponse) {
      for (let i = 0; i < messageUint8.length; i++) {
        if (messageUint8[i] !== HANDSHAKE_RESPONSE_BYTES[i]) {
          isHandshakeResponse = false;
          break;
        }
      }
    }

    if (isHandshakeResponse && this.postMessageHandler_) {
      this.postMessageHandler_.completeHandshake();
    }
  }

  submitQuery(
      query: string, mouseButton: number, altKey: boolean, ctrlKey: boolean,
      metaKey: boolean, shiftKey: boolean) {
    this.methodCalled(
        'submitQuery', query, mouseButton, altKey, ctrlKey, metaKey, shiftKey);
    return Promise.resolve();
  }

  isSidePanelPinned() {
    this.methodCalled('isSidePanelPinned');
    return Promise.resolve({isPinned: false});
  }

  onContextMenuOpened() {
    this.methodCalled('onContextMenuOpened');
  }

  notifySmartTabSharingTryItIphResult(accepted: boolean) {
    this.methodCalled('notifySmartTabSharingTryItIphResult', accepted);
  }

  notifySmartTabSharingDefaultOnIphResult(accepted: boolean) {
    this.methodCalled('notifySmartTabSharingDefaultOnIphResult', accepted);
  }

  registerWindow(
      taskId: ContextualTaskId, url: string, windowId: ContextualWindowId) {
    this.methodCalled('registerWindow', taskId, url, windowId);
  }

  onWindowClosed(windowId: ContextualWindowId) {
    this.methodCalled('onWindowClosed', windowId);
  }

  closeWindow(windowId: ContextualWindowId) {
    this.methodCalled('closeWindow', windowId);
  }
}

/**
 * Test version of the ExtensionPageHandler used to verify calls to the
 * browser from the extension frame.
 */
export class TestExtensionPageHandler extends TestBrowserProxy implements
    ExtensionPageHandlerInterface {
  constructor() {
    super([
      'getHandshakeMessage',
      'getLensCropPreview',
      'onWebviewMessage',
      'removeLensCrop',
      'setTaskId',
      'updateComposeboxHeight',
    ]);
  }

  lensCropPreviewResult: {dataUri: string|null} = {dataUri: null};

  setLensCropPreviewResult(dataUri: string|null) {
    this.lensCropPreviewResult = {dataUri};
  }

  getLensCropPreview() {
    this.methodCalled('getLensCropPreview');
    return Promise.resolve(this.lensCropPreviewResult);
  }

  removeLensCrop() {
    this.methodCalled('removeLensCrop');
    return Promise.resolve();
  }

  getHandshakeMessage(_documentId: string|null = null) {
    this.methodCalled('getHandshakeMessage');
    return Promise.resolve({
      message: {
        protoName: '',
        smuggled: {
          bytes: [1, 2, 3],
        },
      },
    });
  }

  onWebviewMessage(message: number[], _documentId: string|null = null) {
    this.methodCalled('onWebviewMessage', message);
  }

  setTaskId(uuid: Uuid) {
    this.methodCalled('setTaskId', uuid);
  }

  updateComposeboxHeight(height: number) {
    this.methodCalled('updateComposeboxHeight', height);
  }
}

/**
 * Test version of the ExtensionBrowserProxy used in connecting the Contextual
 * Tasks extension frame to the browser.
 */
export class TestExtensionBrowserProxy extends TestBrowserProxy implements
    ExtensionBrowserProxy {
  callbackRouter: ExtensionPageCallbackRouter;
  callbackRouterRemote: ExtensionPageRemote;
  handler: TestExtensionPageHandler;

  constructor() {
    super([]);
    this.callbackRouter = new ExtensionPageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = new TestExtensionPageHandler();
  }
}

/**
 * Test version of the contextual_tasks_toolbar PageHandler used to verify calls
 * to the browser from the toolbar WebUI.
 */
export class TestToolbarPageHandler extends TestBrowserProxy implements
    ToolbarPageHandlerInterface {
  constructor() {
    super([
      'pinSidePanel',
      'unpinSidePanel',
      'openMyActivityUi',
      'openOverflowMenuHelpUi',
      'openManageAccountUi',
      'openLogOutFlow',
      'openFeedbackUi',
      'moveTaskUiToNewTab',
      'showThreadHistory',
      'onFileClickedFromSourcesMenu',
      'onImageClickedFromSourcesMenu',
      'onTabClickedFromSourcesMenu',
      'closeSidePanel',
      'showPageInfoBubble',
      'onLogoPointerDown',
      'createNewThread',
      'maybeTriggerPinningPromo',
    ]);
  }

  pinSidePanel() {
    this.methodCalled('pinSidePanel');
  }

  unpinSidePanel() {
    this.methodCalled('unpinSidePanel');
  }

  openMyActivityUi() {
    this.methodCalled('openMyActivityUi');
  }

  openOverflowMenuHelpUi() {
    this.methodCalled('openOverflowMenuHelpUi');
  }

  openManageAccountUi() {
    this.methodCalled('openManageAccountUi');
  }

  openLogOutFlow() {
    this.methodCalled('openLogOutFlow');
  }

  openFeedbackUi() {
    this.methodCalled('openFeedbackUi');
  }

  moveTaskUiToNewTab() {
    this.methodCalled('moveTaskUiToNewTab');
  }

  showThreadHistory() {
    this.methodCalled('showThreadHistory');
  }

  onTabClickedFromSourcesMenu(tabId: number, url: Url) {
    this.methodCalled('onTabClickedFromSourcesMenu', tabId, url);
  }

  onFileClickedFromSourcesMenu(url: Url) {
    this.methodCalled('onFileClickedFromSourcesMenu', url);
  }

  onImageClickedFromSourcesMenu(url: Url) {
    this.methodCalled('onImageClickedFromSourcesMenu', url);
  }

  closeSidePanel() {
    this.methodCalled('closeSidePanel');
  }

  showPageInfoBubble(isPointerInteraction: boolean) {
    this.methodCalled('showPageInfoBubble', isPointerInteraction);
  }

  onLogoPointerDown() {
    this.methodCalled('onLogoPointerDown');
  }

  createNewThread() {
    this.methodCalled('createNewThread');
  }

  maybeTriggerPinningPromo() {
    this.methodCalled('maybeTriggerPinningPromo');
  }
}

/**
 * Test version of the ContextualTasksToolbarUIService used to verify calls
 * to the browser from the toolbar WebUI.
 */
export class TestContextualTasksToolbarUiService extends TestBrowserProxy
    implements ContextualTasksToolbarUIServiceInterface {
  private initialState_: InitialState = {
    state: {
      indicatorChip: createFakePermissionChip(),
      requestChip: createFakePermissionChip(),
      isDividerVisible: false,
    },
    updateStream: null as any,
  };

  constructor() {
    super([
      'getInitialState',
      'onChipMousePressed',
      'onChipClicked',
      'onChipPointerEntered',
      'onChipPointerExited',
      'onChipExpandAnimationEnded',
      'onChipCollapseAnimationEnded',
    ]);
  }

  setInitialState(state: InitialState) {
    this.initialState_ = state;
  }

  getInitialState(): Promise<InitialState> {
    this.methodCalled('getInitialState');
    return Promise.resolve(this.initialState_);
  }

  onChipMousePressed(identifier: LhsChipIdentifier) {
    this.methodCalled('onChipMousePressed', identifier);
  }

  onChipClicked(
      identifier: LhsChipIdentifier, isMouseInteraction: boolean,
      stateToken: number) {
    this.methodCalled(
        'onChipClicked', [identifier, isMouseInteraction, stateToken]);
  }

  onChipPointerEntered(identifier: LhsChipIdentifier) {
    this.methodCalled('onChipPointerEntered', identifier);
  }

  onChipPointerExited(identifier: LhsChipIdentifier) {
    this.methodCalled('onChipPointerExited', identifier);
  }

  onChipExpandAnimationEnded(identifier: LhsChipIdentifier) {
    this.methodCalled('onChipExpandAnimationEnded', identifier);
  }

  onChipCollapseAnimationEnded(identifier: LhsChipIdentifier) {
    this.methodCalled('onChipCollapseAnimationEnded', identifier);
  }
}

/**
 * Returns a fake PermissionChipState with default values, overridden by
 * `overrides`.
 */
export function createFakePermissionChip(overrides = {}) {
  return {
    isVisible: false,
    isFullyCollapsed: false,
    theme: 0,
    promptStyle: 0,
    userDecision: 0,
    shouldShowBlockedIcon: false,
    iconName: '',
    message: '',
    tooltip: '',
    accessibilityName: '',
    stateToken: 1,
    ...overrides,
  };
}

/**
 * Test version of the ToolbarBrowserProxy used in connecting the Contextual
 * Tasks toolbar WebUI to the browser.
 */
export class TestToolbarBrowserProxy extends TestBrowserProxy implements
    ToolbarBrowserProxy {
  callbackRouter: ToolbarPageCallbackRouter;
  callbackRouterRemote: ToolbarPageRemote;
  handler: TestToolbarPageHandler;
  toolbarUiService: TestContextualTasksToolbarUiService;
  toolbarUiObserverCallbackRouter:
      ContextualTasksToolbarUIObserverCallbackRouter;

  constructor() {
    super([]);
    this.callbackRouter = new ToolbarPageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = new TestToolbarPageHandler();
    this.toolbarUiService = new TestContextualTasksToolbarUiService();
    this.toolbarUiObserverCallbackRouter =
        new ContextualTasksToolbarUIObserverCallbackRouter();
  }

  /**
   * Binds `toolbarUiObserverCallbackRouter` and returns a remote that tests
   * can use to push observer updates to the WebUI.
   */
  bindToolbarUiObserverRemote(): ContextualTasksToolbarUIObserverRemote {
    return this.toolbarUiObserverCallbackRouter.$.bindNewPipeAndPassRemote();
  }
}
