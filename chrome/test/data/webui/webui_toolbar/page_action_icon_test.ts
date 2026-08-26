// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {hexColorToSkColor} from '//resources/js/color_utils.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, PageActionId, PageActionTrigger} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {LhsChipIdentifier, PageActionIconElement, PageActionState} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {BrowserProxy} from 'chrome://webui-toolbar.top-chrome/browser_proxy.js';
import type {ToolbarUIServiceInterface} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api.mojom-webui.js';

class TestToolbarUiHandler extends TestBrowserProxy implements
    ToolbarUIServiceInterface {
  constructor() {
    super([
      'onPageActionClick',
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

suite('PageActionIconTest', function() {
  let icon: PageActionIconElement;
  let toolbarUiHandler: TestToolbarUiHandler;
  let browserProxy: TestToolbarBrowserProxy;

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
    };
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
});
