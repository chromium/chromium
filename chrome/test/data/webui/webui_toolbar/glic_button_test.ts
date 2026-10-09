// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, ContextMenuType, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {GlicButtonElement} from 'chrome://webui-toolbar.top-chrome/app.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

suite('GlicButtonTest', function() {
  let element: GlicButtonElement;
  let browserProxy: TestToolbarBrowserProxy;

  setup(async function() {
    loadTimeData.resetForTesting({
      glicButtonAccName: 'Gemini',
      glicButtonLabel: '',
      glicButtonTooltip: 'Open Gemini',
      glicButtonTooltipClose: 'Close Gemini',
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    element = document.createElement('glic-button');
    document.body.appendChild(element);
    await element.updateComplete;
    await microtasksFinished();
  });

  test('CreatesElementAndChip', function() {
    assertEquals('GLIC-BUTTON', element.tagName);
    assertTrue(!!element.$.button, 'toolbar-chip-button should exist');
    assertEquals('TOOLBAR-CHIP-BUTTON', element.$.button.tagName);

    const icon = element.shadowRoot.querySelector('img#icon');
    assert(icon);
    assertEquals('images/glic_button_alt_icon.png', icon.getAttribute('src'));
  });

  test('DefaultLabelsAndTooltips', function() {
    const chip = element.$.button;
    assertEquals('Open Gemini', chip.tooltip);
    assertEquals('Open Gemini', chip.$.button.getAttribute('title'));
    assertEquals('Gemini', chip.ariaLabel);
    assertEquals('Gemini', chip.$.button.getAttribute('aria-label'));
    assertEquals('dialog', chip.ariaHasPopup);
    assertEquals('dialog', chip.$.button.getAttribute('aria-haspopup'));
    assertEquals('false', chip.ariaExpanded);
    assertEquals('false', chip.$.button.getAttribute('aria-expanded'));
  });

  test('ClickTriggersOnGlicButtonClicked', async function() {
    assertEquals(
        0, browserProxy.toolbarUIHandler.getCallCount('onGlicButtonClicked'));
    assertFalse(element.state.open);

    element.$.button.click();

    await browserProxy.toolbarUIHandler.whenCalled('onGlicButtonClicked');
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('onGlicButtonClicked'));

    // Backend notifies that panel has opened via state update
    element.state = {
      open: true,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    assertTrue(element.state.open);
  });

  test('PanelOpenState', async function() {
    const chip = element.$.button;
    let imgIcon = element.shadowRoot.querySelector('img#icon');
    assert(imgIcon);
    assertFalse(chip.hasAttribute('is-menu-open'));
    assertEquals('false', chip.ariaExpanded);
    assertEquals(
        'images/glic_button_alt_icon.png', imgIcon.getAttribute('src'));

    element.state = {
      open: true,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    const crIcon = element.shadowRoot.querySelector('cr-icon#icon');
    assert(crIcon);
    assertTrue(chip.hasAttribute('is-menu-open'));
    assertEquals('true', chip.ariaExpanded);
    assertEquals('true', chip.$.button.getAttribute('aria-expanded'));
    assertEquals('Close Gemini', chip.tooltip);
    assertEquals('Close Gemini', chip.$.button.getAttribute('title'));
    assertEquals('Close Gemini', chip.ariaLabel);
    assertEquals('Close Gemini', chip.$.button.getAttribute('aria-label'));
    assertEquals('webui-toolbar:glic_button', crIcon.getAttribute('icon'));

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    imgIcon = element.shadowRoot.querySelector('img#icon');
    assert(imgIcon);
    assertFalse(chip.hasAttribute('is-menu-open'));
    assertEquals('false', chip.ariaExpanded);
    assertEquals('Open Gemini', chip.tooltip);
    assertEquals('Gemini', chip.ariaLabel);
    assertEquals(
        'images/glic_button_alt_icon.png', imgIcon.getAttribute('src'));
  });

  test('ContextMenuVisibleHighlightsButton', async function() {
    const chip = element.$.button;
    assertFalse(chip.hasAttribute('is-menu-open'));

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: true,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertTrue(chip.hasAttribute('is-menu-open'));
    assertEquals('false', chip.ariaExpanded);
    assertEquals('Gemini', chip.ariaLabel);

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertFalse(chip.hasAttribute('is-menu-open'));
  });

  test('CustomLabel', async function() {
    const chip = element.$.button;
    assertFalse(chip.hasAttribute('has-label'));
    let text = element.shadowRoot.querySelector('#text');
    assertEquals(null, text);

    element.label = 'Ask Gemini';
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertTrue(chip.hasAttribute('has-label'));
    text = element.shadowRoot.querySelector('#text');
    assert(text);
    assertEquals('Ask Gemini', text.textContent?.trim());
    assertEquals('Ask Gemini', chip.ariaLabel);
  });

  test('NudgeLabelOverridesDefaultLabelAndUpdatesAriaLabel', async function() {
    const chip = element.$.button;
    element.label = 'Ask';
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertEquals(
        'Ask', element.shadowRoot.querySelector('#text')?.textContent?.trim());
    assertEquals('Ask', chip.ariaLabel);

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: 'Summarize this page',
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertTrue(chip.hasAttribute('has-label'));
    assertEquals(
        'Summarize this page',
        element.shadowRoot.querySelector('#text')?.textContent?.trim());
    assertEquals('Summarize this page', chip.ariaLabel);
    assertEquals(
        'Summarize this page', chip.$.button.getAttribute('aria-label'));

    // When open is true, aria-label should be Close Gemini even if nudgeLabel
    // is present.
    element.state = {
      open: true,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: 'Summarize this page',
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();
    assertEquals('Close Gemini', chip.ariaLabel);

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertEquals(
        'Ask', element.shadowRoot.querySelector('#text')?.textContent?.trim());
    assertEquals('Ask', chip.ariaLabel);
  });

  test('DefaultLabelFromLoadTimeData', async function() {
    loadTimeData.overrideValues({glicButtonLabel: 'Ask Gemini'});
    const btn = document.createElement('glic-button');
    document.body.appendChild(btn);
    await btn.updateComplete;
    await btn.$.button.updateComplete;
    await microtasksFinished();

    assertEquals('Ask Gemini', btn.label);
    assertTrue(btn.$.button.hasAttribute('has-label'));
    const text = btn.shadowRoot.querySelector('#text');
    assert(text);
    assertEquals('Ask Gemini', text.textContent?.trim());
    const computedStyle = window.getComputedStyle(text);
    assertEquals('13px', computedStyle.fontSize);
    assertEquals('500', computedStyle.fontWeight);
  });

  test('HelpBubbleIntegration', async function() {
    const chip = element.$.button;
    assertEquals('Open Gemini', chip.tooltip);

    element.hasHelpBubble = true;
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    // Tooltip suppressed during active help bubble
    assertEquals('', chip.tooltip);
    assertTrue(chip.classList.contains('help-anchor-highlight'));

    element.hasHelpBubble = false;
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertEquals('Open Gemini', chip.tooltip);
    assertFalse(chip.classList.contains('help-anchor-highlight'));
  });

  test('DisabledState', async function() {
    const chip = element.$.button;
    assertFalse(chip.disabled);
    assertFalse(chip.$.button.hasAttribute('disabled'));

    element.enabled = false;
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertTrue(chip.disabled);
    assertTrue(chip.$.button.hasAttribute('disabled'));
  });

  test('ContextMenuEventTriggersShowContextMenu', async function() {
    assertEquals(
        0, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));

    const event = new MouseEvent('contextmenu', {
      bubbles: true,
      cancelable: true,
      clientX: 10,
      clientY: 10,
    });
    element.$.button.dispatchEvent(event);
    assertTrue(event.defaultPrevented);

    const [menuType] =
        await browserProxy.toolbarUIHandler.whenCalled('showContextMenu');
    assertEquals(
        1, browserProxy.toolbarUIHandler.getCallCount('showContextMenu'));
    assertEquals(ContextMenuType.kGlic, menuType);
  });

  test('StateUpdatesShouldShow', async function() {
    assertFalse(element.state.shouldShow);
    assertFalse(element.shouldBeShown());

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    assertTrue(element.state.shouldShow);
    assertTrue(element.shouldBeShown());

    element.state = {
      open: false,
      shouldShow: false,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    assertFalse(element.state.shouldShow);
    assertFalse(element.shouldBeShown());
  });

  test('ResponsiveCollapseHidesTextAndShowsOnlyIcon', async function() {
    element.label = 'Ask';
    await element.updateComplete;
    await element.$.button.updateComplete;

    const text = element.shadowRoot.querySelector<HTMLElement>('#text');
    assertTrue(!!text);
    assertTrue(element.$.button.hasAttribute('has-label'));
    assertEquals('block', window.getComputedStyle(text).display);

    element.setToMinWidth();
    assertTrue(element.hasAttribute('collapsed'));
    assertFalse(element.$.button.hasAttribute('has-label'));
    assertEquals('none', window.getComputedStyle(text).display);
    assertEquals(0, element.controlsToAddToOverflowMenu().length);

    element.setToPreferredWidth();
    assertFalse(element.hasAttribute('collapsed'));
    assertTrue(element.$.button.hasAttribute('has-label'));
    assertEquals('block', window.getComputedStyle(text).display);

    let availableWidth = -10;
    Object.defineProperty(element, 'getRootNode', {
      value: () => ({host: {getAvailableWidth: () => availableWidth}}),
      configurable: true,
    });
    element.setToMinWidth();
    element.expandUpToPreferredWidth();
    assertTrue(element.hasAttribute('collapsed'));
    assertFalse(element.$.button.hasAttribute('has-label'));

    availableWidth = 1000;
    element.expandUpToPreferredWidth();
    assertFalse(element.hasAttribute('collapsed'));
    assertTrue(element.$.button.hasAttribute('has-label'));
  });

  test('StateTransitionsWhileCollapsed', async function() {
    element.label = 'Ask Gemini';
    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await element.$.button.updateComplete;

    // Collapse the button.
    element.setToMinWidth();
    assertTrue(element.collapsed);
    assertTrue(element.hasAttribute('collapsed'));
    assertFalse(element.$.button.hasAttribute('has-label'));

    // 2a. Nudge arrives while collapsed: fires request-layout, updates
    // aria-label to the nudge text, and keeps the visual label hidden until
    // expanded.
    let requestLayoutCount = 0;
    element.addEventListener('request-layout', () => {
      requestLayoutCount++;
    });

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: 'Summarize this page',
    };
    await element.updateComplete;
    await element.$.button.updateComplete;

    assertEquals(1, requestLayoutCount);
    assertTrue(element.collapsed);
    assertTrue(element.hasAttribute('collapsed'));
    assertFalse(element.$.button.hasAttribute('has-label'));
    const text = element.shadowRoot.querySelector<HTMLElement>('#text');
    assertTrue(!!text);
    assertEquals('none', window.getComputedStyle(text).display);
    assertEquals('Summarize this page', element.$.button.ariaLabel);

    // 2b. State.open becomes true while collapsed: switches icon, sets
    // is-menu-open and aria-expanded="true", updates aria-label to Close
    // Gemini, and keeps the label hidden.
    element.state = {
      open: true,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    await element.$.button.updateComplete;

    assertTrue(element.collapsed);
    assertTrue(element.hasAttribute('collapsed'));
    assertFalse(element.$.button.hasAttribute('has-label'));
    assertEquals('none', window.getComputedStyle(text).display);
    assertTrue(element.$.button.hasAttribute('is-menu-open'));
    assertEquals('true', element.$.button.ariaExpanded);
    assertEquals('Close Gemini', element.$.button.ariaLabel);
    assertTrue(!!element.shadowRoot.querySelector('cr-icon#icon'));

    // Expanding after state transitions restores the visible label.
    element.setToPreferredWidth();
    await element.updateComplete;
    await element.$.button.updateComplete;
    assertFalse(element.collapsed);
    assertFalse(element.hasAttribute('collapsed'));
    assertTrue(element.$.button.hasAttribute('has-label'));
    assertEquals('block', window.getComputedStyle(text).display);
  });

  test('ClickSuppressionAndTrackedElementActivation', function() {
    const activatedElements: HTMLElement[] = [];
    const originalGetInstance = TrackedElementManager.getInstance;
    const mockManager = {
      notifyElementActivated: (el: HTMLElement) => {
        activatedElements.push(el);
      },
    };
    TrackedElementManager.setInstance(mockManager as any);

    try {
      const dispatchMouseClick = () => {
        element.$.button.dispatchEvent(new PointerEvent(
            'pointerdown', {bubbles: true, button: 0, pointerType: 'mouse'}));
        element.$.button.dispatchEvent(new PointerEvent(
            'click', {bubbles: true, button: 0, pointerType: 'mouse'}));
      };

      // 1. Normal click triggers onGlicButtonClicked and notifies
      // TrackedElementManager.
      dispatchMouseClick();
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('onGlicButtonClicked'));
      assertEquals(1, activatedElements.length);
      assertEquals(element, activatedElements[0]);

      // 2. Click while highlighted is suppressed.
      element.highlightTracker.onHighlightChanged(true);
      dispatchMouseClick();
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('onGlicButtonClicked'));
      assertEquals(1, activatedElements.length);

      // 3. Click immediately after unhighlighting (< 100ms) is suppressed.
      element.highlightTracker.onHighlightChanged(false);
      dispatchMouseClick();
      assertEquals(
          1, browserProxy.toolbarUIHandler.getCallCount('onGlicButtonClicked'));
      assertEquals(1, activatedElements.length);

      // 4. Keyboard activation (empty pointerType) is not suppressed.
      element.$.button.dispatchEvent(new PointerEvent(
          'pointerdown', {bubbles: true, button: 0, pointerType: 'mouse'}));
      element.$.button.dispatchEvent(new PointerEvent(
          'click', {bubbles: true, button: 0, pointerType: ''}));
      assertEquals(
          2, browserProxy.toolbarUIHandler.getCallCount('onGlicButtonClicked'));
      assertEquals(2, activatedElements.length);
    } finally {
      TrackedElementManager.setInstance(undefined);
      TrackedElementManager.getInstance = originalGetInstance;
    }
  });

  test('FiresRequestLayoutOnVisibilityAndLabelChanges', async function() {
    let requestLayoutCount = 0;
    element.addEventListener('request-layout', () => {
      requestLayoutCount++;
    });

    element.state = {
      open: false,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    assertEquals(1, requestLayoutCount);

    // Changing only `open` should not trigger a layout pass.
    element.state = {
      open: true,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: null,
    };
    await element.updateComplete;
    assertEquals(1, requestLayoutCount);

    // Changing `nudgeLabel` should trigger a layout pass.
    element.state = {
      open: true,
      shouldShow: true,
      isContextMenuVisible: false,
      nudgeLabel: 'Summarize',
    };
    await element.updateComplete;
    assertEquals(2, requestLayoutCount);

    // Changing `label` should trigger a layout pass.
    element.label = 'Ask Gemini';
    await element.updateComplete;
    assertEquals(3, requestLayoutCount);
  });
});
