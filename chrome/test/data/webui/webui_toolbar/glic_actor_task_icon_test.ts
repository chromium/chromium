// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/lazy_load.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {GlicActorTaskIconElement} from 'chrome://webui-toolbar.top-chrome/lazy_load.js';
import {BrowserProxyImpl, HighlightTracker, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

suite('GlicActorTaskIconTest', function() {
  let element: GlicActorTaskIconElement;
  let browserProxy: TestToolbarBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    element = document.createElement('glic-actor-task-icon');
    document.body.appendChild(element);
    await element.updateComplete;
    await element.$.button.updateComplete;
    await microtasksFinished();
  });

  test('CreatesElementWithStartFlatEdgeAndCursorSparkIcon', function() {
    assertEquals('GLIC-ACTOR-TASK-ICON', element.tagName);
    const chip = element.$.button;
    assertEquals('TOOLBAR-CHIP-BUTTON', chip.tagName);
    assertEquals('start', chip.flatEdge);
    assertEquals('start', chip.getAttribute('flat-edge'));

    const icon =
        element.shadowRoot.querySelector<CrIconElement>('cr-icon#icon');
    assert(icon);
    assertEquals('prefix-icon', icon.getAttribute('slot'));
    assertEquals('webui-toolbar:cursor_spark', icon.icon);
  });

  test('DefaultIconOnlyStateAndTooltip', async function() {
    const chip = element.$.button;
    assertFalse(chip.hasAttribute('has-label'));
    assertEquals(null, element.shadowRoot.querySelector('#text'));

    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Gemini is working',
      isPressed: false,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertFalse(chip.hasAttribute('has-label'));
    assertEquals(null, element.shadowRoot.querySelector('#text'));
    assertEquals('Gemini is working', chip.tooltip);
    assertEquals('Gemini is working', chip.$.button.getAttribute('title'));
    assertEquals('Gemini is working', chip.ariaLabel);
    assertEquals('Gemini is working', chip.$.button.getAttribute('aria-label'));

    // Like Views, the icon-only button is the button height plus 6px wide,
    // with the icon at the start inset ((34 - 20) / 2 = 7px).
    element.style.setProperty('--toolbar-button-height', '34px');
    element.style.setProperty('--toolbar-button-icon-size', '20px');
    const icon = element.shadowRoot.querySelector<HTMLElement>('#icon');
    assert(icon);
    assertEquals(40, chip.$.button.getBoundingClientRect().width);
    assertEquals(
        7,
        icon.getBoundingClientRect().left -
            chip.$.button.getBoundingClientRect().left);
    // Help bubble suppresses tooltip while preserving ariaLabel and highlights
    // the icon with the feature promo highlight color.
    element.style.setProperty(
        '--color-toolbar-feature-promo-highlight', 'rgb(11, 22, 33)');
    element.hasHelpBubble = true;
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();
    assertEquals('', chip.tooltip);
    assertEquals('Gemini is working', chip.ariaLabel);
    assertTrue(chip.classList.contains('help-anchor-highlight'));
    assertEquals('rgb(11, 22, 33)', getComputedStyle(icon).color);

    element.hasHelpBubble = false;
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();
    assertEquals('Gemini is working', chip.tooltip);
    assertFalse(chip.classList.contains('help-anchor-highlight'));
  });

  test('ActiveNudgeLabelAndRequestLayout', async function() {
    const chip = element.$.button;
    let requestLayoutCount = 0;
    element.addEventListener('request-layout', () => {
      requestLayoutCount++;
    });

    // Showing the icon fires request-layout.
    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Gemini is working',
      isPressed: false,
    };
    await element.updateComplete;
    await chip.updateComplete;
    assertEquals(1, requestLayoutCount);

    // Changing only tooltip or isPressed does not fire request-layout.
    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Updated tooltip',
      isPressed: true,
    };
    await element.updateComplete;
    await chip.updateComplete;
    assertEquals(1, requestLayoutCount);

    // Activating nudgeLabel renders #text, sets has-label, and requests layout.
    element.state = {
      shouldShow: true,
      nudgeLabel: 'Needs review',
      tooltip: 'Needs review',
      isPressed: false,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertEquals(2, requestLayoutCount);
    assertTrue(chip.hasAttribute('has-label'));
    const text = element.shadowRoot.querySelector<HTMLElement>('#text');
    assert(text);
    assertEquals('Needs review', text.textContent.trim());
    assertEquals('Needs review', chip.tooltip);
    assertEquals('Needs review', chip.ariaLabel);

    // Clearing nudgeLabel returns to icon-only state and requests layout.
    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Gemini is working',
      isPressed: false,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertEquals(3, requestLayoutCount);
    assertFalse(chip.hasAttribute('has-label'));
    assertEquals(null, element.shadowRoot.querySelector('#text'));
    assertEquals('Gemini is working', chip.tooltip);
    assertEquals('Gemini is working', chip.ariaLabel);
  });

  test('ResponsiveCollapseHidesNudgeLabelAndShowsOnlyIcon', async function() {
    assertFalse(element.shouldBeShown());
    element.state = {
      shouldShow: true,
      nudgeLabel: 'Check task',
      tooltip: 'Check task',
      isPressed: false,
    };
    await element.updateComplete;
    await element.$.button.updateComplete;
    assertTrue(element.shouldBeShown());

    const text = element.shadowRoot.querySelector<HTMLElement>('#text');
    assertTrue(!!text);
    assertTrue(element.$.button.hasAttribute('has-label'));
    assertEquals('block', window.getComputedStyle(text).display);

    // The toolbar normally provides the button height; set it explicitly so the
    // collapsed minimum width can be checked.
    element.style.setProperty('--toolbar-button-height', '34px');

    element.setToMinWidth();
    assertTrue(element.hasAttribute('collapsed'));
    assertFalse(element.$.button.hasAttribute('has-label'));
    assertEquals('none', window.getComputedStyle(text).display);
    assertEquals(0, element.controlsToAddToOverflowMenu().length);
    // Collapsed, the nudging icon keeps Views' minimum width (height + 6px),
    // with the icon at the start inset.
    element.style.setProperty('--toolbar-button-icon-size', '20px');
    const icon = element.shadowRoot.querySelector<HTMLElement>('#icon');
    assert(icon);
    assertEquals(40, element.$.button.$.button.getBoundingClientRect().width);
    assertEquals(
        7,
        icon.getBoundingClientRect().left -
            element.$.button.$.button.getBoundingClientRect().left);

    element.setToPreferredWidth();
    assertFalse(element.hasAttribute('collapsed'));
    assertTrue(element.$.button.hasAttribute('has-label'));
    assertEquals('block', window.getComputedStyle(text).display);

    // Without a nudge label there is nothing to collapse.
    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Gemini is working',
      isPressed: false,
    };
    await element.updateComplete;
    element.setToMinWidth();
    assertFalse(element.hasAttribute('collapsed'));
  });

  test('PressedStateUpdatesMenuOpenAndAriaAttributes', async function() {
    const chip = element.$.button;
    assertFalse(chip.hasAttribute('is-menu-open'));
    assertEquals('false', chip.ariaExpanded);
    assertEquals('false', chip.$.button.getAttribute('aria-expanded'));
    assertEquals('dialog', chip.ariaHasPopup);
    assertEquals('dialog', chip.$.button.getAttribute('aria-haspopup'));

    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Gemini is working',
      isPressed: true,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertTrue(chip.hasAttribute('is-menu-open'));
    assertEquals('true', chip.ariaExpanded);
    assertEquals('true', chip.$.button.getAttribute('aria-expanded'));
    assertEquals('dialog', chip.ariaHasPopup);
    assertEquals('dialog', chip.$.button.getAttribute('aria-haspopup'));

    element.state = {
      shouldShow: true,
      nudgeLabel: null,
      tooltip: 'Gemini is working',
      isPressed: false,
    };
    await element.updateComplete;
    await chip.updateComplete;
    await microtasksFinished();

    assertFalse(chip.hasAttribute('is-menu-open'));
    assertEquals('false', chip.ariaExpanded);
    assertEquals('false', chip.$.button.getAttribute('aria-expanded'));
  });

  test(
      'ClickTriggersMojoAndAnchorHighlightSuppressesPointerClicks',
      async function() {
        const activatedElements: HTMLElement[] = [];
        const mockManager: Partial<TrackedElementManager> = {
          notifyElementActivated: (el: HTMLElement) => {
            activatedElements.push(el);
          },
        };
        TrackedElementManager.setInstance(mockManager as TrackedElementManager);

        try {
          const dispatchPointerClick = () => {
            element.$.button.dispatchEvent(new PointerEvent('pointerdown', {
              bubbles: true,
              button: 0,
              pointerType: 'mouse',
            }));
            element.$.button.dispatchEvent(new PointerEvent('click', {
              bubbles: true,
              button: 0,
              detail: 1,
              pointerType: 'mouse',
            }));
          };

          // 1. Left pointer click when unpressed invokes
          // onGlicActorTaskIconClicked().
          dispatchPointerClick();
          await browserProxy.toolbarUIHandler.whenCalled(
              'onGlicActorTaskIconClicked');
          assertEquals(
              1,
              browserProxy.toolbarUIHandler.getCallCount(
                  'onGlicActorTaskIconClicked'));
          assertEquals(1, activatedElements.length);
          assertEquals(element, activatedElements[0]);

          // 2. While the task list bubble's anchor highlight is on this
          // element, a pointer click is suppressed.
          element.highlightTracker.onHighlightChanged(true);
          dispatchPointerClick();
          assertEquals(
              1,
              browserProxy.toolbarUIHandler.getCallCount(
                  'onGlicActorTaskIconClicked'));
          assertEquals(1, activatedElements.length);

          // 3. Keyboard activation (detail === 0 / empty pointerType) while
          // highlighted still invokes onGlicActorTaskIconClicked().
          element.$.button.dispatchEvent(new PointerEvent('click', {
            bubbles: true,
            button: 0,
            detail: 0,
            pointerType: '',
          }));
          assertEquals(
              2,
              browserProxy.toolbarUIHandler.getCallCount(
                  'onGlicActorTaskIconClicked'));
          assertEquals(2, activatedElements.length);

          // 4. Removing the highlight suppresses an immediate pointer click
          // within the unhighlight race window. Use a large threshold so slow
          // bots can't fall outside the window.
          HighlightTracker.setClickSuppressionThresholdForTesting(60000);
          element.highlightTracker.onHighlightChanged(false);
          dispatchPointerClick();
          assertEquals(
              2,
              browserProxy.toolbarUIHandler.getCallCount(
                  'onGlicActorTaskIconClicked'));

          // 5. Once outside the suppression window, pointer clicks invoke the
          // handler again.
          HighlightTracker.setClickSuppressionThresholdForTesting(0);
          dispatchPointerClick();
          assertEquals(
              3,
              browserProxy.toolbarUIHandler.getCallCount(
                  'onGlicActorTaskIconClicked'));
        } finally {
          HighlightTracker.setClickSuppressionThresholdForTesting(100);
          TrackedElementManager.setInstance(undefined);
        }
      });

  test('MouseActivatesOnPressAndTouchOnTap', function() {
    // A left mouse press invokes the handler immediately, matching Views'
    // MenuButtonController, and the click that follows is ignored.
    element.$.button.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      button: 0,
      pointerType: 'mouse',
    }));
    assertEquals(
        1,
        browserProxy.toolbarUIHandler.getCallCount(
            'onGlicActorTaskIconClicked'));
    element.$.button.dispatchEvent(new PointerEvent('click', {
      bubbles: true,
      button: 0,
      detail: 1,
      pointerType: 'mouse',
    }));
    assertEquals(
        1,
        browserProxy.toolbarUIHandler.getCallCount(
            'onGlicActorTaskIconClicked'));

    // A touch press does nothing; the tap (click) invokes the handler.
    element.$.button.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      button: 0,
      pointerType: 'touch',
    }));
    assertEquals(
        1,
        browserProxy.toolbarUIHandler.getCallCount(
            'onGlicActorTaskIconClicked'));
    element.$.button.dispatchEvent(new PointerEvent('click', {
      bubbles: true,
      button: 0,
      detail: 1,
      pointerType: 'touch',
    }));
    assertEquals(
        2,
        browserProxy.toolbarUIHandler.getCallCount(
            'onGlicActorTaskIconClicked'));
  });

  test('ArrowKeysActivateOnKeydown', function() {
    for (const [i, key] of ['ArrowDown', 'ArrowUp'].entries()) {
      const event = new KeyboardEvent('keydown', {key, cancelable: true});
      element.$.button.dispatchEvent(event);
      assertTrue(event.defaultPrevented);
      assertEquals(
          i + 1,
          browserProxy.toolbarUIHandler.getCallCount(
              'onGlicActorTaskIconClicked'));
    }

    // Other keys do not activate on keydown.
    for (const key of ['ArrowLeft', 'Enter', ' ']) {
      const event = new KeyboardEvent('keydown', {key, cancelable: true});
      element.$.button.dispatchEvent(event);
      assertFalse(event.defaultPrevented);
      assertEquals(
          2,
          browserProxy.toolbarUIHandler.getCallCount(
              'onGlicActorTaskIconClicked'));
    }
  });

  test('NudgeLabelUsesLabelForegroundColor', async function() {
    element.style.setProperty('--color-label-foreground', 'rgb(1, 2, 3)');
    element.style.setProperty('--color-toolbar-button-text', 'rgb(4, 5, 6)');
    element.state = {
      shouldShow: true,
      nudgeLabel: 'Check task',
      tooltip: 'Check task',
      isPressed: false,
    };
    await element.updateComplete;
    await microtasksFinished();

    const text = element.shadowRoot.querySelector<HTMLElement>('#text');
    assert(text);
    assertEquals('rgb(1, 2, 3)', window.getComputedStyle(text).color);
  });

  test('IconMirroredInRtl', function() {
    const icon = element.shadowRoot.querySelector<HTMLElement>('#icon');
    assert(icon);
    assertEquals('none', window.getComputedStyle(icon).transform);

    const oldDir = document.documentElement.dir;
    try {
      document.documentElement.dir = 'rtl';
      assertEquals(
          'matrix(-1, 0, 0, 1, 0, 0)', window.getComputedStyle(icon).transform);
    } finally {
      document.documentElement.dir = oldDir;
    }
  });
});
