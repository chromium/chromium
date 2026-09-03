// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {browserProxyFactory} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import type {HelpBubbleHandlerInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TrackedElementIdentifier} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE, resetInitialStateForTesting, SearchboxBrowserProxy, SecurityChipRole, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {LhsChipIdentifier, ToolbarAppElement} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy, FocusRequestListener, NavigationControlsStateListener} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';
import {AvatarToolbarButtonState} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api_data_model.mojom-webui.js';

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

  addShowSplitTabsContextMenuListener() {
    return 0;
  }

  removeNavigationStateListener(handle: number) {
    this.methodCalled('removeNavigationStateListener', handle);
    this.listener_ = null;
  }

  removeFocusRequestListener(handle: number) {
    this.methodCalled('removeFocusRequestListener', handle);
  }

  removeShowSplitTabsContextMenuListener() {}

  onChipClicked(_chip: LhsChipIdentifier, _isPointerClick: boolean) {}
  onChipPointerEntered(_chip: LhsChipIdentifier) {}
  onChipPointerExited(_chip: LhsChipIdentifier) {}
  onChipMousePressed(_chip: LhsChipIdentifier) {}
  onChipExpandAnimationEnded(_chip: LhsChipIdentifier) {}
  onChipCollapseAnimationEnded(_chip: LhsChipIdentifier) {}

  fireNavigationStateListener(iconUpdates: any[], state: any) {
    if (this.listener_) {
      this.listener_(iconUpdates, state);
    }
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

  helpBubbleButtonPressed(id: TrackedElementIdentifier, button: number) {
    this.methodCalled('helpBubbleButtonPressed', id, button);
  }

  helpBubbleClosed(id: TrackedElementIdentifier, reason: any) {
    this.methodCalled('helpBubbleClosed', id, reason);
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
      windowIsMaximizedOrFullscreen: false,
    },
    homeControlState: {
      shouldBeShown: false,
      isContextMenuVisible: false,
    },
    locationBarState: {
      omniboxViewState: {
        textPieces: [],
        inlineAutocompletion: '',
        a11yFriendlySuggestionText: '',
        selection: null,
        textIsUrl: false,
      },
      locationBarFlags: {
        userInputInProgress: false,
        popupOpen: false,
      },
      contentSettingImageStates: [],
      pageActionStates: [],
      lhsChipsState: {
        securityChip: {
          icon: {handleId: 0n},
          securityLevel: 0,
          text: '',
          tooltip: '',
          isClickable: false,
          isTextDangerous: false,
          isVisible: true,
          accessibilityState: {
            role: SecurityChipRole.kButton,
            label: '',
            description: '',
          },
        },
        activityIndicators: [],
        permissionDashboard: null,
      },
    },
    avatarControlState: {
      state: AvatarToolbarButtonState.kNormal,
      icon: {handleId: 0n},
      text: '',
      tooltip: '',
      accessibilityName: '',
      accessibilityDescription: '',
      enabled: true,
      hasLinearGradientRing: false,
    },
    layoutConstantsVersion: 0,
    pinnedToolbarActionsState: [],
  };
}

suite('ToolbarAppTest', () => {
  let app: ToolbarAppElement;
  let browserProxy: TestToolbarBrowserProxy;
  let originalGetVariableValue: any;

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

    const handler = new TestHelpBubbleHandler();
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    SearchboxBrowserProxy.setInstance(new TestSearchboxBrowserProxy());

    // Reset C++ injected values to ensure tests start in a clean state.
    const loadTimeDataData = (loadTimeData as any).data_;
    if (loadTimeDataData) {
      delete loadTimeDataData['isNavigationLoading'];
      delete loadTimeDataData['reloadCanShowMenu'];
      delete loadTimeDataData['backButtonEnabled'];
      delete loadTimeDataData['forwardButtonEnabled'];
      delete loadTimeDataData['homeButtonShouldBeShown'];
      delete loadTimeDataData['batterySaverButtonVisible'];
      delete loadTimeDataData['layoutConstantsVersion'];
      delete loadTimeDataData['touchUi'];
      delete loadTimeDataData['isFallbackPrewarming'];
    }

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

    originalGetVariableValue = chrome.getVariableValue;
    chrome.getVariableValue = (key: string) => {
      if (key === 'initialState') {
        const state: Record<string, any> = {};
        const keys = [
          'isNavigationLoading',
          'backButtonEnabled',
          'forwardButtonEnabled',
          'initialWebUISurfaceSyncEnabled',
          'isFallbackPrewarming',
          'reloadCanShowMenu',
          'homeButtonShouldBeShown',
          'batterySaverButtonVisible',
          'layoutConstantsVersion',
          'touchUi',
        ];
        for (const k of keys) {
          if (loadTimeData.valueExists(k)) {
            state[k] = loadTimeData.getValue(k);
          }
        }
        return JSON.stringify(state);
      }
      return originalGetVariableValue(key);
    };

    resetInitialStateForTesting();
  });

  teardown(() => {
    chrome.getVariableValue = originalGetVariableValue;
    resetInitialStateForTesting();
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
      isFallbackPrewarming: true,
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

  test('Sync Enabled - Synchronous Boot (Initial State Present)', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
      isFallbackPrewarming: false,
      isNavigationLoading: false,
      backButtonEnabled: true,
      forwardButtonEnabled: false,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);

    await microtasksFinished();

    // Verify app initializes synchronously without needing a Mojo update
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onPageInitialized'));
    assertEquals(9, startTrackingCalls.length);

    // Verify initial boot snapshot captures the correct values
    const snapshot = (app as any).initialBootSnapshot_;
    assertTrue(snapshot.initializedSync);
    assertFalse(snapshot.isNavigationLoading);
    assertTrue(snapshot.backButtonEnabled);
    assertFalse(snapshot.forwardButtonEnabled);
  });

  test('Sync Enabled - Synchronous Mojo Update', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: true,
      isFallbackPrewarming: true,
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
      isFallbackPrewarming: true,
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
      isFallbackPrewarming: true,
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
      isFallbackPrewarming: true,
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
          isFallbackPrewarming: true,
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
      isFallbackPrewarming: true,
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
      isFallbackPrewarming: true,
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

  test('AvatarButtonHighlightClasses', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: false,
    });
    const highlightClasses = [
      'highlight-default',
      'highlight-sync-error',
      'highlight-guest',
      'highlight-incognito',
    ];

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    await microtasksFinished();

    const avatarButton = app.shadowRoot.querySelector('avatar-button')!;
    const innerButton = avatarButton.shadowRoot.querySelector('#button')!;

    // Helper to update state and check class
    const checkClass = async (state: any, expectedClass: string) => {
      const navigationState = createMockNavigationState();
      navigationState.avatarControlState = state;
      browserProxy.fireNavigationStateListener([], navigationState);
      await microtasksFinished();
      if (expectedClass) {
        assertTrue(
            innerButton.classList.contains(expectedClass),
            `Expected class ${expectedClass} for state ${state.state}`);
        // Ensure other highlight classes are not present
        for (const cls of highlightClasses) {
          if (cls !== expectedClass) {
            assertFalse(
                innerButton.classList.contains(cls),
                `Expected NOT to have class ${cls} for state ${state.state}`);
          }
        }
      } else {
        // Should have no highlight classes
        for (const cls of highlightClasses) {
          assertFalse(
              innerButton.classList.contains(cls),
              `Expected no highlight class for state ${state.state}`);
        }
      }
    };

    // No text -> no highlight
    await checkClass(
        {
          state: AvatarToolbarButtonState.kNormal,
          text: '',
          iconUrl: '',
          tooltip: '',
          accessibilityName: '',
          accessibilityDescription: '',
        },
        '');

    // Normal state with text -> highlight-default
    await checkClass(
        {
          state: AvatarToolbarButtonState.kNormal,
          text: 'Profile',
          iconUrl: '',
          tooltip: '',
          accessibilityName: '',
          accessibilityDescription: '',
        },
        'highlight-default');

    // Sync error with text -> highlight-sync-error
    await checkClass(
        {
          state: AvatarToolbarButtonState.kSyncError,
          text: 'Error',
          iconUrl: '',
          tooltip: '',
          accessibilityName: '',
          accessibilityDescription: '',
        },
        'highlight-sync-error');

    // Guest with text -> highlight-guest
    await checkClass(
        {
          state: AvatarToolbarButtonState.kGuestSession,
          text: 'Guest',
          iconUrl: '',
          tooltip: '',
          accessibilityName: '',
          accessibilityDescription: '',
        },
        'highlight-guest');

    // Incognito with text -> highlight-incognito
    await checkClass(
        {
          state: AvatarToolbarButtonState.kIncognitoProfile,
          text: 'Incognito',
          iconUrl: '',
          tooltip: '',
          accessibilityName: '',
          accessibilityDescription: '',
        },
        'highlight-incognito');
  });

  test('AvatarButtonLinearGradientRing', async () => {
    loadTimeData.overrideValues({
      initialWebUISurfaceSyncEnabled: false,
    });

    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    await microtasksFinished();

    const avatarButton = app.shadowRoot.querySelector('avatar-button')!;
    const innerButton = avatarButton.shadowRoot.querySelector('#button')!;

    // Helper to update state and check attribute
    const checkLinearGradientRing = async (hasLinearGradientRing: boolean) => {
      const navigationState = createMockNavigationState();
      navigationState.avatarControlState = {
        state: AvatarToolbarButtonState.kNormal,
        text: 'Profile',
        icon: {handleId: 0n},
        tooltip: '',
        accessibilityName: '',
        accessibilityDescription: '',
        enabled: true,
        hasLinearGradientRing: hasLinearGradientRing,
      };
      browserProxy.fireNavigationStateListener([], navigationState);
      await microtasksFinished();
      const icon = avatarButton.shadowRoot.querySelector('#icon')!;
      const iconStyle = window.getComputedStyle(icon);
      const actualButton = innerButton.shadowRoot!.querySelector('#button')!;
      const buttonStyle = window.getComputedStyle(actualButton);

      if (hasLinearGradientRing) {
        assertTrue(innerButton.hasAttribute('has-linear-gradient-ring'));
        assertEquals('30px', iconStyle.width);
        assertEquals('30px', iconStyle.height);
        assertEquals('5px', buttonStyle.paddingLeft);
        assertEquals('7px', buttonStyle.paddingRight);
      } else {
        assertFalse(innerButton.hasAttribute('has-linear-gradient-ring'));
        assertEquals('20px', iconStyle.width);
        assertEquals('20px', iconStyle.height);
        assertEquals('10px', buttonStyle.paddingLeft);
        assertEquals('12px', buttonStyle.paddingRight);
      }
    };

    await checkLinearGradientRing(false);
    await checkLinearGradientRing(true);
  });

  test('GetAvailableWidth', async () => {
    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    await microtasksFinished();

    assertEquals(window.innerWidth - app.clientWidth, app.getAvailableWidth());
  });

  test('AvatarButtonAnchorHighlightDoesNotPulse', async () => {
    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    await microtasksFinished();

    const avatarButton = app.shadowRoot.querySelector('avatar-button')!;
    const innerChip = avatarButton.shadowRoot.querySelector('#button')!;
    const visualTarget =
        innerChip.shadowRoot!.querySelector('.iph-visual-target')!;
    assertTrue(!!visualTarget);

    avatarButton.classList.add('anchor-highlight');
    await microtasksFinished();

    assertEquals(
        'none',
        window.getComputedStyle(visualTarget, '::before').animationName);
    assertEquals(
        '1',
        getComputedStyle(innerChip)
            .getPropertyValue('--toolbar-chip-highlight-opacity')
            .trim());

    avatarButton.classList.remove('anchor-highlight');
  });

  test('AvatarButtonHelpBubbleActivatesPulseAnimation', async () => {
    app = document.createElement('toolbar-app');
    document.body.appendChild(app);
    await microtasksFinished();

    const avatarButton = app.shadowRoot.querySelector('avatar-button')!;
    const innerChip = avatarButton.shadowRoot.querySelector('#button')!;
    const visualTarget =
        innerChip.shadowRoot!.querySelector('.iph-visual-target')!;
    assertTrue(!!visualTarget);

    avatarButton.hasHelpBubble = true;
    await microtasksFinished();

    assertTrue(innerChip.classList.contains('help-anchor-highlight'));
    assertEquals(
        'pulse',
        window.getComputedStyle(visualTarget, '::before').animationName);
    assertEquals(
        '1', window.getComputedStyle(visualTarget, '::before').opacity);

    avatarButton.hasHelpBubble = false;
    await microtasksFinished();

    assertFalse(innerChip.classList.contains('help-anchor-highlight'));
  });
});
