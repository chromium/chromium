// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {HelpBubbleClientCallbackRouter} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import type {HelpBubbleHandlerInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {HelpBubbleProxyImpl} from 'chrome://resources/cr_components/help_bubble/help_bubble_proxy.js';
import type {HelpBubbleProxy} from 'chrome://resources/cr_components/help_bubble/help_bubble_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TrackedElementHandlerInterface} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ToolbarAppElement} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy, FocusRequestListener, NavigationControlsStateListener} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';

class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['onPageInitialized']);
  }

  onPageInitialized() {
    this.methodCalled('onPageInitialized');
  }
}

class TestBrowserControlsHandler extends TestBrowserProxy {
  constructor() {
    super(['navigate']);
  }

  navigate(url: any, disposition: any) {
    this.methodCalled('navigate', url, disposition);
  }
}

class TestToolbarBrowserProxy extends TestBrowserProxy implements BrowserProxy {
  toolbarUIHandler: any;
  browserControlsHandler: any;
  private listener_: NavigationControlsStateListener|null = null;

  constructor() {
    super([
      'recordInHistogram',
      'addNavigationStateListener',
      'addFocusRequestListener',
      'removeNavigationStateListener',
      'removeFocusRequestListener',
    ]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
    this.browserControlsHandler = new TestBrowserControlsHandler();
  }

  recordInHistogram() {}

  addNavigationStateListener(listener: NavigationControlsStateListener) {
    this.methodCalled('addNavigationStateListener', listener);
    this.listener_ = listener;
    return 1;
  }

  addFocusRequestListener(listener: FocusRequestListener) {
    this.methodCalled('addFocusRequestListener', listener);
    return INVALID_FOCUS_REQUEST_HANDLE;
  }

  removeNavigationStateListener(handle: number) {
    this.methodCalled('removeNavigationStateListener', handle);
    this.listener_ = null;
  }

  removeFocusRequestListener(handle: number) {
    this.methodCalled('removeFocusRequestListener', handle);
  }

  fireNavigationStateListener(iconUpdates: any[], state: any) {
    if (this.listener_) {
      this.listener_(iconUpdates, state);
    }
  }
}

class TestTrackedElementHandler extends TestBrowserProxy implements
    TrackedElementHandlerInterface {
  constructor() {
    super([
      'setManager',
      'trackedElementVisibilityChanged',
      'trackedElementActivated',
      'trackedElementCustomEvent',
      'trackedElementCanHighlightChanged',
    ]);
  }

  setManager(_manager: any) {
    this.methodCalled('setManager');
  }

  trackedElementVisibilityChanged(nativeIdentifier: string, visible: boolean) {
    this.methodCalled(
        'trackedElementVisibilityChanged', nativeIdentifier, visible);
  }

  trackedElementActivated(nativeIdentifier: string) {
    this.methodCalled('trackedElementActivated', nativeIdentifier);
  }

  trackedElementCustomEvent(nativeIdentifier: string, eventName: string) {
    this.methodCalled('trackedElementCustomEvent', nativeIdentifier, eventName);
  }

  trackedElementCanHighlightChanged(
      nativeIdentifier: string, canHighlight: boolean) {
    this.methodCalled(
        'trackedElementCanHighlightChanged', nativeIdentifier, canHighlight);
  }
}

class TestHelpBubbleHandler extends TestBrowserProxy implements
    HelpBubbleHandlerInterface {
  constructor() {
    super([
      'bindTrackedElementHandler',
      'helpBubbleButtonPressed',
      'helpBubbleClosed',
    ]);
  }

  bindTrackedElementHandler(_handler: any) {
    this.methodCalled('bindTrackedElementHandler');
  }

  helpBubbleButtonPressed(nativeIdentifier: string, button: number) {
    this.methodCalled('helpBubbleButtonPressed', nativeIdentifier, button);
  }

  helpBubbleClosed(nativeIdentifier: string, reason: any) {
    this.methodCalled('helpBubbleClosed', nativeIdentifier, reason);
  }
}

class TestHelpBubbleProxy implements HelpBubbleProxy {
  private testTrackedElementHandler_ = new TestTrackedElementHandler();
  private testHandler_ = new TestHelpBubbleHandler();
  private callbackRouter_ = new HelpBubbleClientCallbackRouter();

  getTrackedElementHandler() {
    return this.testTrackedElementHandler_;
  }

  getHandler() {
    return this.testHandler_;
  }

  getCallbackRouter() {
    return this.callbackRouter_;
  }
}

function createMockNavigationState() {
  return {
    reloadControlState: {
      doubleClickInterval: {microseconds: 500000n},
      canShowMenu: false,
      isNavigationLoading: false,
      isContextMenuVisible: false,
      stateToken: 0,
    },
    splitTabsControlState: {
      isCurrentTabSplit: false,
      location: 0,
      isPinned: false,
      isContextMenuVisible: false,
    },
    backForwardControlState: {
      backButtonState:
          {enabled: false, shouldBeShown: true, isContextMenuVisible: false},
      forwardButtonState:
          {enabled: false, shouldBeShown: true, isContextMenuVisible: false},
      backButtonLeadingMargin: 0,
    },
    homeControlState: {
      shouldBeShown: false,
      isContextMenuVisible: false,
    },
    locationBarState: {
      omniboxViewState: {
        textPieces: [],
        inlineAutocompletion: '',
        selection: null,
        textIsUrl: false,
      },
      locationBarFlags: {
        userInputInProgress: false,
        popupOpen: false,
      },
      contentSettingImageStates: [],
      lhsChipsState: {
        securityChip: {
          icon: {handleId: 0n},
          securityLevel: 0,
          text: '',
          isClickable: false,
          isTextDangerous: false,
          isVisible: true,
          accessibilityState: {
            label: '',
            description: '',
          },
        },
        activityIndicators: [],
        permissionDashboard: null,
      },
    },
    avatarControlState: {
      iconUrl: '',
      text: '',
      tooltip: '',
      accessibilityName: '',
      accessibilityDescription: '',
    },
    layoutConstantsVersion: 0,
    pinnedToolbarActionsState: [],
  };
}

suite('ToolbarAppTest', () => {
  let app: ToolbarAppElement;
  let browserProxy: TestToolbarBrowserProxy;
  let helpBubbleProxy: TestHelpBubbleProxy;
  let startTrackingCalls: Array<[HTMLElement, string]> = [];
  let stopTrackingCalls: HTMLElement[] = [];

  const mockManager = {
    startTracking: (element: HTMLElement, nativeId: string) => {
      startTrackingCalls.push([element, nativeId]);
    },
    stopTracking: (element: HTMLElement) => {
      stopTrackingCalls.push(element);
    },
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    startTrackingCalls = [];
    stopTrackingCalls = [];
    TrackedElementManager.setInstance(mockManager as any);

    helpBubbleProxy = new TestHelpBubbleProxy();
    HelpBubbleProxyImpl.setInstance(helpBubbleProxy);

    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      enableReloadButton: true,
      enableSplitTabsButton: true,
      enableHomeButton: true,
      enableLocationBar: true,
      enableBackForwardButtons: true,
      enablePinnedToolbarActions: true,
      enableAvatarButton: true,
      splitTabsIndicatorWidth: 10,
      splitTabsIndicatorHeight: 10,
      splitTabsIndicatorSpacing: 10,
    });
  });

  test('Sync Disabled (Initial Sync Feature is False)', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: false,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    await microtasksFinished();

    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length);
  });

  test('Sync Enabled (Initial Sync Feature is True)', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    await microtasksFinished();

    // Verify app remains uninitialized
    assertEquals(
        0, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(0, startTrackingCalls.length);

    // Trigger visual state update (simulate browser sync)
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    await microtasksFinished();

    // Verify visual update triggered initialization
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));

    // Assert that elements are tracked. This will fail on unfixed codebase.
    assertEquals(9, startTrackingCalls.length);
  });

  test('Sync Enabled - Synchronous Mojo Update', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    // Fire update synchronously before microtasks finish
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    await microtasksFinished();

    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length);
  });

  test('Sync Enabled - Multiple Rapid Mojo Updates', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    browserProxy.fireNavigationStateListener([], createMockNavigationState());
    browserProxy.fireNavigationStateListener([], createMockNavigationState());
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    await microtasksFinished();

    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length);
  });

  test('Sync Enabled - Synchronous Detach Reattach', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    // Fire Mojo update which schedules initialization
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    // Detach and re-attach synchronously before microtask runs
    document.body.removeChild(app);
    document.body.appendChild(app);

    // Since it was re-attached, it needs a new Mojo update to initialize
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    await microtasksFinished();

    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length);
  });

  test('Sync Enabled - Asynchronous Detach Reattach', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    // Fire Mojo update
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    // Let initialization finish
    await microtasksFinished();
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length);

    // Detach and verify cleanup
    document.body.removeChild(app);
    assertEquals(9, stopTrackingCalls.length);

    // Re-attach
    document.body.appendChild(app);
    await microtasksFinished();

    // Verify it is not initialized yet (since it's waiting for Mojo update)
    assertEquals(11, startTrackingCalls.length);
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));

    // Fire Mojo update to trigger initialization on reconnect
    browserProxy.fireNavigationStateListener([], createMockNavigationState());
    await microtasksFinished();

    assertEquals(20, startTrackingCalls.length);
    assertEquals(
        2, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
  });

  test(
      'Sync Enabled - Detach Before Child Elements Update Complete',
      async () => {
        loadTimeData.overrideValues({
          initialWebUISurfaceSyncEnabled: true,
        });

        app = document.createElement('toolbar-app');
        document.body.appendChild(app);

        // Fire Mojo update which schedules initialization
        browserProxy.fireNavigationStateListener(
            [], createMockNavigationState());

        // Wait for the app's own update to complete, so initializePage_ is
        // called
        await app.updateComplete;

        // Detach immediately, before the child elements' updateComplete
        // promises resolve
        document.body.removeChild(app);

        await microtasksFinished();

        // Verify that onPageInitialized was NOT called because the element is
        // detached
        assertEquals(
            0, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
      });

  test('Event Listener Clean Up on Detach', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    const activeScrollListeners = new Set<Function>();
    const originalAdd = document.addEventListener;
    const originalRemove = document.removeEventListener;

    document.addEventListener = function(
        type: string, listener: any, options?: any) {
      if (type === 'scroll') {
        activeScrollListeners.add(listener);
      }
      originalAdd.call(document, type, listener, options);
    };

    document.removeEventListener = function(
        type: string, listener: any, options?: any) {
      if (type === 'scroll') {
        activeScrollListeners.delete(listener);
      }
      originalRemove.call(document, type, listener, options);
    };

    try {
      const initialListenersCount = activeScrollListeners.size;

      app = document.createElement('toolbar-app');
      document.body.appendChild(app);

      // Fire Mojo update
      browserProxy.fireNavigationStateListener([], createMockNavigationState());
      await microtasksFinished();

      // Verify page is initialized
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));

      // Detach
      document.body.removeChild(app);
      await microtasksFinished();

      // Verify that all scroll listeners added by the app (via
      // HelpBubbleMixinLit) are cleaned up.
      assertEquals(initialListenersCount, activeScrollListeners.size);
    } finally {
      document.addEventListener = originalAdd;
      document.removeEventListener = originalRemove;
    }
  });

  test('Sync Enabled - Multiple Rapid Reconnects and Updates', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
    });

    // We will attach and detach the app multiple times rapidly, interleaved
    // with Mojo updates.
    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    browserProxy.fireNavigationStateListener([], createMockNavigationState());
    document.body.removeChild(app);

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    browserProxy.fireNavigationStateListener([], createMockNavigationState());
    browserProxy.fireNavigationStateListener([], createMockNavigationState());
    document.body.removeChild(app);

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    browserProxy.fireNavigationStateListener([], createMockNavigationState());

    await microtasksFinished();

    // Verify only the final instance got initialized successfully
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length - stopTrackingCalls.length);
  });
});
