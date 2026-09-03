// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import type {HelpBubbleOptions} from '//resources/cr_components/help_bubble/help_bubble_controller.js';
import {hexColorToSkColor} from '//resources/js/color_utils.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, IconTable, IconType, PageActionId, PageActionTrigger, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {LhsChipIdentifier, PageActionIconElement, PageActionState} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';
import type {ToolbarUIServiceInterface} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api.mojom-webui.js';

class TestToolbarUiHandler extends TestBrowserProxy implements
    ToolbarUIServiceInterface {
  constructor() {
    super([
      'onPageActionClick',
      'onPageActionPointerDown',
    ]);
  }

  bind() {
    return new Promise<never>(() => {});
  }
  showContextMenu() {}
  showOverflowMenu() {
    return Promise.resolve({result: {}});
  }
  onOmniboxAction() {
    return new Promise<never>(() => {});
  }
  onPageInitialized() {}
  onContentSettingImagePointerDown() {}
  onPageActionPointerDown(actionId: PageActionId) {
    this.methodCalled('onPageActionPointerDown', actionId);
  }
  showContentSettingsBubble() {
    return new Promise<never>(() => {});
  }
  onContentSettingImageAnimationEnded() {}
  invokePinnedToolbarAction() {}
  onHomeButtonDropUrl() {}
  onHomeButtonDropFile() {}
  onToolbarDropFile() {}
  showAvatarMenu() {
    return new Promise<never>(() => {});
  }
  setAvatarButtonHovered(_hovered: boolean) {
    return Promise.resolve({result: {}});
  }
  setAvatarButtonFocused(_focused: boolean) {
    return Promise.resolve({result: {}});
  }
  setAvatarButtonIphPromoShowing(_showing: boolean) {
    return Promise.resolve({result: {}});
  }
  onAppMenuFocusChanged(_focused: boolean) {}
  onLocationBarFocusWithinChanged(_focusInside: boolean) {}
  onLhsChipMousePressed(_id: LhsChipIdentifier, _isMiddleClick: boolean) {}
  onLhsChipClicked() {}
  onLhsChipCollapseAnimationEnded() {}
  onLhsChipExpandAnimationEnded() {}
  onLhsChipPointerEntered() {}
  onLhsChipPointerExited() {}
  onLhsChipDrag() {}
  movePinnedToolbarAction(_actionId: any, _targetIndex: any) {}
  movePinnedToolbarActionBy(_actionId: any, _delta: any) {}
  moveExtensionAction(_extensionId: string, _targetIndex: number) {}
  moveExtensionActionBy(_extensionId: string, _delta: number) {}

  onPageActionClick(actionId: PageActionId, trigger: PageActionTrigger) {
    this.methodCalled('onPageActionClick', [actionId, trigger]);
    return Promise.resolve({result: {}});
  }

  onPageActionChipShowingChanged(_actionId: PageActionId) {
    return Promise.resolve({result: {}});
  }

  executeExtensionAction(_extensionId: string) {}

  showExtensionContextMenu(_extensionId: string, _source: any) {}

  adjustOmniboxTextForCopy(text: string, _selectionStart: number) {
    return Promise.resolve({
      adjustedText: text,
      adjustedUrl: null,
      pageTitle: null,
    });
  }

  onPerformanceInterventionButtonClicked(_isMouseInteraction: boolean) {}

  onPerformanceInterventionButtonMousePressed() {}
}

class TestToolbarBrowserProxy extends TestBrowserProxy implements BrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;
  browserControlsHandler: any;  // Not used in this test

  constructor() {
    super([]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
  }

  recordInHistogram() {}
  addNavigationStateListener() {
    return 0;
  }
  addFocusRequestListener() {
    return 0;
  }
  removeNavigationStateListener() {}
  removeFocusRequestListener() {}

  onChipClicked(_chip: LhsChipIdentifier, _isPointerClick: boolean) {}
  onChipPointerEntered(_chip: LhsChipIdentifier) {}
  onChipPointerExited(_chip: LhsChipIdentifier) {}
  onChipMousePressed(_chip: LhsChipIdentifier) {}
  onChipExpandAnimationEnded(_chip: LhsChipIdentifier) {}
  onChipCollapseAnimationEnded(_chip: LhsChipIdentifier) {}
}

interface StartTrackingCall {
  element: HTMLElement;
  nativeId: string;
  options?: HelpBubbleOptions;
}

suite('PageActionIconTest', function() {
  let icon: PageActionIconElement;
  let toolbarUiHandler: TestToolbarUiHandler;
  let browserProxy: TestToolbarBrowserProxy;
  let startTrackingCalls: StartTrackingCall[] = [];
  let stopTrackingCalls: HTMLElement[] = [];

  function createBaseState(): PageActionState {
    return {
      pageActionId: PageActionId.kActionShowTranslate,
      accessibleName: 'Translate',
      tooltipText: 'Translate this page',
      icon: {handleId: 0n},
      text: '',
      shouldShowChip: false,
      shouldAnimateChipIn: false,
      shouldAnimateChipOut: false,
      backgroundColorOverride: null,
      identifier: {
        nativeIdentifier: '',
        secondaryIdentifier: '',
      },
      isActive: false,
      iconAnimationToken: 0,
    };
  }

  /**
   * Helper that yields execution to the browser for two animation frames.
   * This is used to ensure that any requestAnimationFrame callbacks scheduled
   * by the implementation (such as triggering SVG SMIL icon animations) have
   * completed and the browser has initialized the icon animation state before
   * the test code queries or interacts with it.
   */
  function nextFrame(): Promise<void> {
    return new Promise(resolve => {
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          resolve();
        });
      });
    });
  }

  /**
   * Helper that finds the SMIL animate element in the given icon, dispatches
   * the 'endEvent' to simulate the icon animation finishing, and asserts that
   * the element resets back to the static icon.
   */
  async function triggerAnimationEndAndVerify(animatedIcon: CrIconElement) {
    const animate =
        animatedIcon.shadowRoot?.querySelector('animate, animateTransform');
    assertTrue(!!animate, 'animate element should be found');
    animate.dispatchEvent(new Event('endEvent'));

    await icon.updateComplete;

    assertTrue(!!icon.shadowRoot.querySelector('icon-from-table'));
    assertTrue(!icon.shadowRoot.querySelector('#animatedIcon'));
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    startTrackingCalls = [];
    stopTrackingCalls = [];

    const mockManager: Partial<TrackedElementManager> = {
      startTracking:
          (element: HTMLElement, nativeId: string,
           options?: HelpBubbleOptions) => {
            startTrackingCalls.push({element, nativeId, options});
          },
      stopTracking: (element: HTMLElement) => {
        stopTrackingCalls.push(element);
      },
    };
    TrackedElementManager.setInstance(mockManager as TrackedElementManager);

    browserProxy = new TestToolbarBrowserProxy();
    toolbarUiHandler = browserProxy.toolbarUIHandler;
    BrowserProxyImpl.setInstance(browserProxy);

    icon = document.createElement('page-action-icon');
    icon.state = createBaseState();
    document.body.appendChild(icon);
    await microtasksFinished();
  });

  test('Render', function() {
    const button = icon.$.button;
    assertEquals('Translate this page', button.tooltip);
    assertEquals('Translate', button.ariaLabel);
  });

  test('Background color', async function() {
    const button = icon.$.button;
    const actualButton = button.$.button;
    icon.style.setProperty(
        '--color-omnibox-icon-background-tonal', 'rgb(255, 0, 0)');

    // Default: transparent.
    const style = actualButton.computedStyleMap();
    assertEquals('rgba(0, 0, 0, 0)', style.get('background-color')?.toString());

    // Show a text chip, should now use --color-omnibox-icon-background-tonal
    icon.state = {
      ...createBaseState(),
      text: 'Inexplicably expanded bookmark chip',
      shouldShowChip: true,
    };
    await microtasksFinished();

    assertEquals('rgb(255, 0, 0)', style.get('background-color')?.toString());

    // Custom color, no chip.
    icon.state = {
      ...createBaseState(),
      backgroundColorOverride: hexColorToSkColor('#00ff00'),
    };
    await microtasksFinished();

    assertEquals('rgb(0, 255, 0)', style.get('background-color')?.toString());

    // Custom color, with chip -- still using custom color.
    icon.state = {
      ...createBaseState(),
      text: 'Inexplicably expanded bookmark chip',
      shouldShowChip: true,
      backgroundColorOverride: hexColorToSkColor('#0000ff'),
    };
    await microtasksFinished();

    assertEquals('rgb(0, 0, 255)', style.get('background-color')?.toString());

    // And back to no custom color again.
    icon.state = {
      ...createBaseState(),
      text: 'Inexplicably expanded bookmark chip',
      shouldShowChip: true,
    };
    await microtasksFinished();

    assertEquals('rgb(255, 0, 0)', style.get('background-color')?.toString());
  });

  test('Render without accessible name', async function() {
    icon.state = {
      ...icon.state,
      accessibleName: '',
    };
    await microtasksFinished();
    const button = icon.$.button;
    // Should NOT fallback to tooltipText
    assertEquals('', button.ariaLabel || '');
  });

  test('Mouse click triggers Action with kMouse', async function() {
    const button = icon.$.button;
    button.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      composed: true,
      detail: 1,
    }));

    const [actionId, trigger] =
        await toolbarUiHandler.whenCalled('onPageActionClick');
    assertEquals(PageActionId.kActionShowTranslate, actionId);
    assertEquals(PageActionTrigger.kMouse, trigger);
  });

  test('Keyboard click triggers Action with kKeyboard', async function() {
    const button = icon.$.button;
    // button.click() triggers click with detail: 0, simulating keyboard.
    button.click();

    const [actionId, trigger] =
        await toolbarUiHandler.whenCalled('onPageActionClick');
    assertEquals(PageActionId.kActionShowTranslate, actionId);
    assertEquals(PageActionTrigger.kKeyboard, trigger);
  });

  test('Touch tap triggers Action with kGesture', async function() {
    const button = icon.$.button;
    button.dispatchEvent(new PointerEvent('click', {
      bubbles: true,
      composed: true,
      pointerType: 'touch',
    }));

    const [actionId, trigger] =
        await toolbarUiHandler.whenCalled('onPageActionClick');
    assertEquals(PageActionId.kActionShowTranslate, actionId);
    assertEquals(PageActionTrigger.kGesture, trigger);
  });

  test('Pen click triggers Action with kGesture', async function() {
    const button = icon.$.button;
    button.dispatchEvent(new PointerEvent('click', {
      bubbles: true,
      composed: true,
      pointerType: 'pen',
    }));

    const [actionId, trigger] =
        await toolbarUiHandler.whenCalled('onPageActionClick');
    assertEquals(PageActionId.kActionShowTranslate, actionId);
    assertEquals(PageActionTrigger.kGesture, trigger);
  });

  test('Pointer events fired', function() {
    const button = icon.$.button;

    let enterFired = false;
    icon.addEventListener('chip-pointerenter', () => {
      enterFired = true;
    });
    button.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(enterFired);

    let leaveFired = false;
    icon.addEventListener('chip-pointerleave', () => {
      leaveFired = true;
    });
    button.dispatchEvent(new PointerEvent('pointerleave'));
    assertTrue(leaveFired);

    let cancelFired = false;
    icon.addEventListener('chip-pointercancel', () => {
      cancelFired = true;
    });
    button.dispatchEvent(new PointerEvent('pointercancel'));
    assertTrue(cancelFired);
  });

  test('Does not animate label without text', async function() {
    icon.state = {
      ...createBaseState(),
      text: '',
      shouldShowChip: false,
      shouldAnimateChipOut: true,
    };
    await microtasksFinished();
    assertFalse(icon.$.button.hasAttribute('animates-label'));
    assertFalse(icon.$.button.hasAttribute('has-label'));
  });

  test(
      'Does not animate label on initial mount in collapsed state',
      async function() {
        const newIcon = document.createElement('page-action-icon');
        newIcon.state = {
          ...createBaseState(),
          text: 'Chip text',
          shouldShowChip: false,
          shouldAnimateChipOut: true,
        };
        document.body.appendChild(newIcon);
        await microtasksFinished();
        assertFalse(newIcon.$.button.hasAttribute('animates-label'));
        assertFalse(newIcon.$.button.hasAttribute('has-label'));
      });

  test('Animates label when expanding chip', async function() {
    icon.state = {
      ...createBaseState(),
      text: 'Chip text',
      shouldShowChip: true,
      shouldAnimateChipIn: true,
    };
    await microtasksFinished();
    assertTrue(icon.$.button.hasAttribute('animates-label'));
    assertTrue(icon.$.button.hasAttribute('has-label'));
  });

  test('Animates label when collapsing from active chip', async function() {
    icon.state = {
      ...createBaseState(),
      text: 'Chip text',
      shouldShowChip: true,
      shouldAnimateChipIn: true,
    };
    await microtasksFinished();
    assertTrue(icon.$.button.hasAttribute('animates-label'));
    assertTrue(icon.$.button.hasAttribute('has-label'));

    // Collapse chip
    icon.state = {
      ...icon.state,
      shouldShowChip: false,
      shouldAnimateChipOut: true,
    };
    await microtasksFinished();
    assertTrue(icon.$.button.hasAttribute('animates-label'));
    assertFalse(icon.$.button.hasAttribute('has-label'));
  });

  test(
      'Does not animate label when switching to different action id',
      async function() {
        icon.state = {
          ...createBaseState(),
          pageActionId: PageActionId.kActionAiMode,
          text: 'AI Mode',
          shouldShowChip: true,
          shouldAnimateChipIn: true,
        };
        await microtasksFinished();
        assertTrue(icon.$.button.hasAttribute('animates-label'));
        assertTrue(icon.$.button.hasAttribute('has-label'));

        // Update state to a different action ID that is collapsed (e.g.
        // Bookmark)
        icon.state = {
          ...createBaseState(),
          pageActionId: PageActionId.kActionBookmarkThisTab,
          text: 'Bookmark this tab',
          shouldShowChip: false,
          shouldAnimateChipOut: true,
        };
        await microtasksFinished();
        assertFalse(icon.$.button.hasAttribute('animates-label'));
        assertFalse(icon.$.button.hasAttribute('has-label'));
      });

  test('Pointer down triggers onPageActionPointerDown', async function() {
    const button = icon.$.button;
    button.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      composed: true,
    }));

    const actionId =
        await toolbarUiHandler.whenCalled('onPageActionPointerDown');
    assertEquals(PageActionId.kActionShowTranslate, actionId);
  });

  test('Start tracking when identifier is set', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();

    assertEquals(1, startTrackingCalls.length);
    assertEquals(icon.$.button, startTrackingCalls[0]!.element);
    assertEquals('test-id', startTrackingCalls[0]!.nativeId);
    assertEquals(0, stopTrackingCalls.length);
  });

  test('Stop tracking when identifier is cleared', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: '',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();

    assertEquals(1, startTrackingCalls.length);
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(icon.$.button, stopTrackingCalls[0]!);
  });

  test('Stop tracking and start tracking when identifier changes', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id-1',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id-2',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();

    assertEquals(1, stopTrackingCalls.length);
    assertEquals(icon.$.button, stopTrackingCalls[0]!);
    assertEquals(2, startTrackingCalls.length);
    assertEquals(icon.$.button, startTrackingCalls[1]!.element);
    assertEquals('test-id-2', startTrackingCalls[1]!.nativeId);
  });

  test('Stop tracking on disconnected', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);

    icon.remove();
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(icon.$.button, stopTrackingCalls[0]!);
  });

  test('Highlight changed callback updates is-menu-open', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);
    const options = startTrackingCalls[0]!.options;
    assertTrue(!!options?.onHighlightChanged);

    assertFalse(icon.$.button.hasAttribute('is-menu-open'));

    options.onHighlightChanged(true);
    await microtasksFinished();
    assertTrue(icon.$.button.hasAttribute('is-menu-open'));

    options.onHighlightChanged(false);
    await microtasksFinished();
    assertFalse(icon.$.button.hasAttribute('is-menu-open'));
  });

  test('Help bubble callbacks update hasHelpBubble and tooltip', async () => {
    icon.state = {
      ...icon.state,
      identifier: {
        nativeIdentifier: 'test-id',
        secondaryIdentifier: '',
      },
    };
    await microtasksFinished();
    assertEquals(1, startTrackingCalls.length);
    const options = startTrackingCalls[0]!.options;
    assertTrue(!!options?.onHelpBubbleShown);
    assertTrue(!!options?.onHelpBubbleHidden);

    assertFalse(icon.hasHelpBubble);
    assertEquals(
        'Translate this page',
        icon.adjustTooltipForHelpBubble('Translate this page'));

    options.onHelpBubbleShown();
    await microtasksFinished();
    assertTrue(icon.hasHelpBubble);
    assertEquals('', icon.adjustTooltipForHelpBubble('Translate this page'));

    options.onHelpBubbleHidden();
    await microtasksFinished();
    assertFalse(icon.hasHelpBubble);
    assertEquals(
        'Translate this page',
        icon.adjustTooltipForHelpBubble('Translate this page'));
  });

  test('isActive in state updates is-menu-open', async () => {
    assertFalse(icon.$.button.hasAttribute('is-menu-open'));

    icon.state = {
      ...icon.state,
      isActive: true,
    };
    await microtasksFinished();
    assertTrue(icon.$.button.hasAttribute('is-menu-open'));

    icon.state = {
      ...icon.state,
      isActive: false,
    };
    await microtasksFinished();
    assertFalse(icon.$.button.hasAttribute('is-menu-open'));
  });

  test('Glow up icon animation on bookmark star', async function() {
    icon.glowUpEnabled = true;

    const iconTable = IconTable.getInstance();
    iconTable.applyUpdates([
      {
        handleId: 1n,
        iconUrlOrName: 'webui-toolbar:star',
        iconType: IconType.kIconSet,
        color: null,
      },
      {
        handleId: 2n,
        iconUrlOrName: 'webui-toolbar:star_filled',
        iconType: IconType.kIconSet,
        color: null,
      },
    ]);

    icon.state = {
      ...createBaseState(),
      pageActionId: PageActionId.kActionBookmarkThisTab,
      icon: {handleId: 1n},
    };
    await microtasksFinished();

    assertTrue(!!icon.shadowRoot.querySelector('icon-from-table'));
    assertTrue(!icon.shadowRoot.querySelector('#animatedIcon'));

    // Transition to starred
    icon.state = {
      ...icon.state,
      icon: {handleId: 2n},
    };
    await icon.updateComplete;

    const animatedIcon =
        icon.shadowRoot.querySelector<CrIconElement>('#animatedIcon');
    assertTrue(!!animatedIcon);
    await animatedIcon.updateComplete;

    // Wait for the requestAnimationFrame in playIconAnimation_ to run and
    // attach the listener.
    await nextFrame();

    assertEquals('webui-toolbar:star_glow_up', animatedIcon.icon);
    assertTrue(
        !icon.shadowRoot.querySelector('icon-from-table'),
        'icon-from-table should not be present');

    await triggerAnimationEndAndVerify(animatedIcon);
  });

  test('Glow up icon animation on bookmark unstar', async function() {
    icon.glowUpEnabled = true;

    const iconTable = IconTable.getInstance();
    iconTable.applyUpdates([
      {
        handleId: 1n,
        iconUrlOrName: 'webui-toolbar:star',
        iconType: IconType.kIconSet,
        color: null,
      },
      {
        handleId: 2n,
        iconUrlOrName: 'webui-toolbar:star_filled',
        iconType: IconType.kIconSet,
        color: null,
      },
    ]);

    // Initial state: starred
    icon.state = {
      ...createBaseState(),
      pageActionId: PageActionId.kActionBookmarkThisTab,
      icon: {handleId: 2n},
    };
    await microtasksFinished();

    assertTrue(!!icon.shadowRoot.querySelector('icon-from-table'));
    assertTrue(!icon.shadowRoot.querySelector('#animatedIcon'));

    // Transition to unstarred
    icon.state = {
      ...icon.state,
      icon: {handleId: 1n},
    };
    await icon.updateComplete;

    const animatedIcon =
        icon.shadowRoot.querySelector<CrIconElement>('#animatedIcon');
    assertTrue(!!animatedIcon);
    await animatedIcon.updateComplete;

    // Wait for the requestAnimationFrame in playIconAnimation_ to run and
    // attach the listener.
    await nextFrame();

    assertEquals('webui-toolbar:star_filled_glow_up', animatedIcon.icon);
    assertTrue(
        !icon.shadowRoot.querySelector('icon-from-table'),
        'icon-from-table should not be present');

    await triggerAnimationEndAndVerify(animatedIcon);
  });

  test(
      'No glow up icon animation on tab switch or navigation',
      async function() {
        icon.glowUpEnabled = true;

        const iconTable = IconTable.getInstance();
        iconTable.applyUpdates([
          {
            handleId: 1n,
            iconUrlOrName: 'webui-toolbar:star',
            iconType: IconType.kIconSet,
            color: null,
          },
          {
            handleId: 2n,
            iconUrlOrName: 'webui-toolbar:star_filled',
            iconType: IconType.kIconSet,
            color: null,
          },
        ]);

        // Initial state: not bookmarked, token 1
        icon.state = {
          ...createBaseState(),
          pageActionId: PageActionId.kActionBookmarkThisTab,
          icon: {handleId: 1n},
          iconAnimationToken: 1,
        };
        await microtasksFinished();

        assertTrue(!!icon.shadowRoot.querySelector('icon-from-table'));
        assertTrue(!icon.shadowRoot.querySelector('#animatedIcon'));

        // Transition to starred, but with a different icon animation token (tab
        // switch/navigation)
        icon.state = {
          ...icon.state,
          icon: {handleId: 2n},
          iconAnimationToken: 2,
        };
        await icon.updateComplete;

        // Wait a couple of frames to ensure no animation was deferred and
        // triggered
        await nextFrame();

        // Verify it remains on static icon and no animated icon is rendered
        assertTrue(!!icon.shadowRoot.querySelector('icon-from-table'));
        assertTrue(!icon.shadowRoot.querySelector('#animatedIcon'));
      });
});
