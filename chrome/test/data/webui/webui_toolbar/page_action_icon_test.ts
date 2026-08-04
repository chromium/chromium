// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
  onOmniboxAction() {
    return new Promise<never>(() => {});
  }
  onPageInitialized() {}
  onContentSettingImagePointerDown() {}
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
  onLhsChipMousePressed() {}
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
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);
    assertEquals('Translate this page', button.getAttribute('title'));
    assertEquals('Translate', button.getAttribute('aria-label'));
  });

  test('Render without accessible name', async function() {
    icon.state = {
      ...icon.state,
      accessibleName: '',
    };
    await microtasksFinished();
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);
    // Should NOT fallback to tooltipText
    assertEquals('', button.getAttribute('aria-label') || '');
  });

  test('Mouse click triggers Action with kMouse', async function() {
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);
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
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);
    // button.click() triggers click with detail: 0, simulating keyboard.
    button.click();

    const [actionId, trigger] =
        await toolbarUiHandler.whenCalled('onPageActionClick');
    assertEquals(PageActionId.kActionShowTranslate, actionId);
    assertEquals(PageActionTrigger.kKeyboard, trigger);
  });

  test('Touch tap triggers Action with kGesture', async function() {
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);
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
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);
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
    const button = icon.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!button);

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
});
