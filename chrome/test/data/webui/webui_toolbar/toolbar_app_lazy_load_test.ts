// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {browserProxyFactory} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import type {HelpBubbleHandlerInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TrackedElementIdentifier} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {AppMenuIconType, AppMenuSeverity, AvatarToolbarButtonState, BrowserProxyImpl, createDefaultLocationBarState, IconTable, IconType, PageActionAnimationStyle, PageActionId, PinnedToolbarAction, PinnedToolbarActionEngagedIndicatorStyle, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';
import type {ToolbarStateListener} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {TestToolbarBrowserProxy as BaseTestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

class TestToolbarBrowserProxy extends BaseTestToolbarBrowserProxy {
  private listener_: ToolbarStateListener|null = null;

  override addToolbarStateListener(listener: ToolbarStateListener) {
    this.methodCalled('addToolbarStateListener', listener);
    this.listener_ = listener;
    return 1;
  }

  override removeToolbarStateListener(handle: number) {
    this.methodCalled('removeToolbarStateListener', handle);
    this.listener_ = null;
  }

  fireToolbarStateListener(iconUpdates: any[], state: any) {
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

function hasRenderedTableIcon(host: Element|null|undefined): boolean {
  return !!host?.shadowRoot?.querySelector('icon-from-table')
               ?.shadowRoot?.querySelector('cr-icon')
               ?.shadowRoot?.querySelector('svg');
}

suite('ToolbarAppLazyLoadTest', () => {
  // Tests that when <toolbar-app> receives ToolbarState before lazy_load.js
  // executes, importing lazy_load.js registers all toolbar iconsets before
  // upgrading connected <avatar-button>, <location-bar>, and
  // <pinned-toolbar-actions> elements.
  test('UpgradesConnectedControlsWithIconsetIcons', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assertFalse(!!customElements.get('location-bar'));

    TrackedElementManager.setInstance({
      startTracking: () => {},
      stopTracking: () => {},
      notifyElementActivated: () => {},
    } as any);

    const handler = new TestHelpBubbleHandler();
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    const browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      enableReloadButton: true,
      enableSplitTabsButton: true,
      enableHomeButton: true,
      enableLocationBar: true,
      enableBackForwardButtons: true,
      enablePinnedToolbarActions: true,
      enableAvatarButton: true,
      toolbarAppMenuLabelResizingEnabled: true,
      toolbarGlicButtonResizingEnabled: true,
      splitTabsIndicatorWidth: 10,
      splitTabsIndicatorHeight: 10,
      splitTabsIndicatorSpacing: 10,
    });

    const origRequestIdleCallback = window.requestIdleCallback;
    window.requestIdleCallback = () => 0;
    const errors: Event[] = [];
    const onError = (e: Event) => errors.push(e);
    window.addEventListener('error', onError);
    window.addEventListener('unhandledrejection', onError);

    let locationIconName = 'webui-toolbar:search';
    let keywordIconName = 'webui-toolbar:notes_spark';
    let pageActionIconName = 'webui-toolbar:star';
    let pinnedIconName = 'webui-toolbar:code_16';
    // <if expr="_google_chrome">
    locationIconName = 'internal-icons:google_g_logo_monochrome';
    keywordIconName = 'internal-icons:google_g_logo';
    pageActionIconName = 'internal-icons:google_lens_monochrome_logo';
    pinnedIconName = 'internal-icons:audio_spark';
    // </if>

    const iconUpdates = [
      {
        handleId: 1n,
        iconUrlOrName: locationIconName,
        iconType: IconType.kIconSet,
        color: null,
      },
      {
        handleId: 2n,
        iconUrlOrName: keywordIconName,
        iconType: IconType.kIconSet,
        color: null,
      },
      {
        handleId: 3n,
        iconUrlOrName: pageActionIconName,
        iconType: IconType.kIconSet,
        color: null,
      },
      {
        handleId: 4n,
        iconUrlOrName: 'webui-toolbar-shared:code_24',
        iconType: IconType.kIconSet,
        color: null,
      },
      {
        handleId: 5n,
        iconUrlOrName: pinnedIconName,
        iconType: IconType.kIconSet,
        color: null,
      },
    ];

    const locationBarState = createDefaultLocationBarState();
    locationBarState.lhsChipsState.securityChip.icon = {handleId: 1n};
    locationBarState.lhsChipsState.securityChip.isVisible = true;
    locationBarState.pageActionStates = [{
      pageActionId: PageActionId.kActionShowTranslate,
      accessibleName: 'Translate',
      tooltipText: 'Translate',
      icon: {handleId: 3n},
      text: 'Translate',
      shouldShowChip: true,
      shouldAnimateChipIn: false,
      shouldAnimateChipOut: false,
      backgroundColorOverride: null,
      foregroundColorOverride: null,
      identifier: {nativeIdentifier: '', secondaryIdentifier: ''},
      isActive: false,
      tabSwitchToken: 0,
      animationStyle: PageActionAnimationStyle.kStandard,
      trailingIcon: null,
      showTrailingIcon: false,
    }];

    const toolbarState = {
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
      locationBarState,
      avatarControlState: {
        state: AvatarToolbarButtonState.kNormal,
        icon: {handleId: 4n},
        text: '',
        tooltip: '',
        accessibilityName: '',
        accessibilityDescription: '',
        enabled: true,
        hasLinearGradientRing: false,
      },
      appMenuControlState: {
        iconType: AppMenuIconType.kNone,
        severity: AppMenuSeverity.kNone,
        labelText: null as string | null,
        accessibilityText: '',
        tooltip: '',
        isContextMenuVisible: false,
        windowIsMaximizedOrFullscreen: false,
      },
      glicButtonState: {
        open: false,
        shouldShow: false,
        isContextMenuVisible: false,
        shouldMatchToolbarStyle: false,
        useCustomThemeFallback: false,
        nudgeLabel: null as string | null,
        isSplit: false,
      },
      glicActorTaskIconState: {
        shouldShow: false,
        nudgeLabel: null as string | null,
        tooltip: '',
        isPressed: false,
      },
      overflowButtonControlState: {
        isContextMenuVisible: false,
      },
      layoutConstantsVersion: 0,
      pinnedToolbarActionsState: [{
        action: PinnedToolbarAction.kDevTools,
        highlighted: false,
        enabled: true,
        activated: false,
        engagedIndicatorStyle:
            PinnedToolbarActionEngagedIndicatorStyle.kUnderline,
        tooltip: 'DevTools',
        accessibilityText: 'DevTools',
        elementId: 'devtools',
        icon: {handleId: 5n},
        progressRing: null,
        badgeNumber: null,
        iconSizeDelta: 0,
      }],
      controlsAppearInactive: false,
    };

    try {
      const app = document.createElement('toolbar-app');
      document.body.appendChild(app);

      browserProxy.fireToolbarStateListener(iconUpdates, toolbarState);
      await microtasksFinished();
      assertFalse(!!customElements.get('location-bar'));

      await import('chrome://webui-toolbar.top-chrome/lazy_load.js');
      await microtasksFinished();

      assertEquals(0, errors.length);
      assertTrue(!!customElements.get('location-bar'));

      assertTrue(
          hasRenderedTableIcon(app.shadowRoot.querySelector('avatar-button')));
      const locationBar = app.shadowRoot.querySelector('location-bar');
      assertTrue(!!locationBar);
      assertTrue(hasRenderedTableIcon(
          locationBar.shadowRoot.querySelector('location-icon')));
      assertTrue(hasRenderedTableIcon(
          locationBar.shadowRoot.querySelector('page-action-icons')
              ?.shadowRoot?.querySelector('page-action-icon')));
      assertTrue(hasRenderedTableIcon(
          app.shadowRoot.querySelector('pinned-toolbar-actions')
              ?.shadowRoot?.querySelector('pinned-toolbar-action')));

      browserProxy.fireToolbarStateListener([], {
        ...toolbarState,
        locationBarState: {
          ...toolbarState.locationBarState,
          selectedKeyword: {
            shortName: 'Google',
            fullName: 'Google',
            icon: {handleId: 2n},
          },
        },
      });
      await microtasksFinished();

      assertEquals(0, errors.length);
      assertTrue(hasRenderedTableIcon(
          locationBar.shadowRoot.querySelector('selected-keyword')));
    } finally {
      window.requestIdleCallback = origRequestIdleCallback;
      window.removeEventListener('error', onError);
      window.removeEventListener('unhandledrejection', onError);
      IconTable.getInstance().reset();
    }
  });
});
