// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_aim_button.js';

import type {ComposeboxAimButtonElement} from 'chrome://resources/cr_components/composebox/composebox_aim_button.js';
import {AimButtonMode} from 'chrome://resources/cr_components/composebox/composebox_aim_button.js';
import {assertEquals, assertFalse, assertNear, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

const FOCUS_OUTLINE_CLASS = 'focus-outline-visible';

function assertModifiers(
    e: KeyboardEvent|MouseEvent, expected: EventModifierInit = {}) {
  assertEquals(expected.altKey ?? false, e.altKey);
  assertEquals(expected.ctrlKey ?? false, e.ctrlKey);
  assertEquals(expected.metaKey ?? false, e.metaKey);
  assertEquals(expected.shiftKey ?? false, e.shiftKey);
}

async function createButton(): Promise<ComposeboxAimButtonElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const element = document.createElement('cr-composebox-aim-button');
  element.label = 'AI Mode';
  element.exitTitle = 'Exit AI Mode';
  element.sendTitle = 'Send';
  // Finish transitions instantly so state assertions are deterministic.
  for (const token of ['short', 'long']) {
    element.style.setProperty(`--aim-motion-duration-${token}`, '0ms');
  }
  document.body.appendChild(element);
  await microtasksFinished();
  return element;
}

function recordClicks(element: ComposeboxAimButtonElement):
    Array<KeyboardEvent|MouseEvent> {
  const events: Array<KeyboardEvent|MouseEvent> = [];
  element.addEventListener('aim-button-click', e => {
    events.push((e as CustomEvent<KeyboardEvent|MouseEvent>).detail);
  });
  return events;
}

function dispatchKey(
    target: Element, type: 'keydown'|'keyup', key: string,
    init: KeyboardEventInit = {}) {
  target.dispatchEvent(new KeyboardEvent(
      type, {bubbles: true, cancelable: true, composed: true, key, ...init}));
}

async function setState(
    element: ComposeboxAimButtonElement, mode: AimButtonMode,
    showExitIcon: boolean) {
  element.mode = mode;
  element.showExitIcon = showExitIcon;
  await element.updateComplete;
}

function assertIconShown(icon: HTMLElement) {
  const style = getComputedStyle(icon);
  assertEquals('visible', style.visibility);
  assertNear(20, parseFloat(style.width), 0.1);
}

function assertIconCollapsed(icon: HTMLElement) {
  const style = getComputedStyle(icon);
  assertEquals('hidden', style.visibility);
  assertEquals('0px', style.width);
}

suite('ComposeboxAimButtonTest', () => {
  let element: ComposeboxAimButtonElement;

  setup(async () => {
    element = await createButton();
  });

  test('ClickFiresActivationAndTitleFollowsMode', async () => {
    const button = element.$.button;
    const events = recordClicks(element);

    // Exit mode: button click.
    assertEquals('Exit AI Mode', button.title);
    assertEquals('Exit AI Mode', button.getAttribute('aria-label'));
    const label = $$(element, '#label');
    assertTrue(!!label);
    assertEquals('AI Mode', label.textContent);
    button.click();
    assertEquals(1, events.length);

    await setState(element, AimButtonMode.EXIT, true);
    const trailingIcon = $$(element, '#trailingIcon');
    assertTrue(!!trailingIcon);
    assertFalse(trailingIcon.hasAttribute('title'));
    trailingIcon.click();
    assertEquals(2, events.length);

    // Send mode.
    await setState(element, AimButtonMode.SEND, false);
    assertEquals('Send', button.title);
    assertEquals('Send', button.getAttribute('aria-label'));
    button.click();
    assertEquals(3, events.length);

    // Titles track the properties.
    element.sendTitle = 'Send now';
    await element.updateComplete;
    assertEquals('Send now', button.title);
    assertEquals('Send now', button.getAttribute('aria-label'));
  });

  test('ModeSwitchKeepsButtonAndFocusAndReusesTrailingIcon', async () => {
    const button = element.$.button;
    element.focus();
    assertEquals(button, element.shadowRoot.activeElement);

    await setState(element, AimButtonMode.EXIT, true);
    const trailingIcon = $$(element, '#trailingIcon');
    assertTrue(!!trailingIcon);
    assertTrue(trailingIcon.classList.contains('icon-clear'));

    await setState(element, AimButtonMode.SEND, false);
    assertEquals(button, $$(element, '#button'));
    assertEquals(button, element.shadowRoot.activeElement);
    assertEquals(trailingIcon, $$(element, '#trailingIcon'));
    assertTrue(trailingIcon.classList.contains('arrow-icon'));
    assertFalse(trailingIcon.classList.contains('icon-clear'));

    await setState(element, AimButtonMode.EXIT, false);
    assertEquals(button, $$(element, '#button'));
    assertEquals(button, element.shadowRoot.activeElement);
    assertIconCollapsed(trailingIcon);

    element.blur();
    assertEquals(null, element.shadowRoot.activeElement);
  });

  test('KeyboardActivationCarriesModifiers', async () => {
    const button = element.$.button;
    const events = recordClicks(element);

    // Enter activates on keydown.
    dispatchKey(button, 'keydown', 'Enter', {shiftKey: true, altKey: true});
    dispatchKey(button, 'keyup', 'Enter', {shiftKey: true, altKey: true});
    assertEquals(1, events.length);
    assertTrue(events[0] instanceof KeyboardEvent);
    assertModifiers(events[0], {shiftKey: true, altKey: true});

    // Space activates on keyup.
    await setState(element, AimButtonMode.SEND, false);
    dispatchKey(button, 'keydown', ' ', {ctrlKey: true, metaKey: true});
    assertEquals(1, events.length);
    dispatchKey(button, 'keyup', ' ', {ctrlKey: true, metaKey: true});
    assertEquals(2, events.length);
    assertTrue(events[1] instanceof KeyboardEvent);
    assertModifiers(events[1], {ctrlKey: true, metaKey: true});

    // A held-down Enter repeat does not activate, and its modifiers must not
    // leak into a later click.
    dispatchKey(button, 'keydown', 'Enter', {repeat: true, shiftKey: true});
    assertEquals(2, events.length);
    button.click();
    assertEquals(3, events.length);
    assertTrue(events[2] instanceof MouseEvent);
    assertModifiers(events[2]);

    // Same for an unpaired Space keyup.
    dispatchKey(button, 'keyup', ' ', {altKey: true});
    assertEquals(3, events.length);
    button.click();
    assertEquals(4, events.length);
    assertTrue(events[3] instanceof MouseEvent);
    assertModifiers(events[3]);

    // A mouse click on the trailing icon uses the mouse event's own modifiers,
    // even with a stale keyboard event around.
    dispatchKey(button, 'keyup', ' ', {altKey: true});
    const trailingIcon = $$(element, '#trailingIcon');
    assertTrue(!!trailingIcon);
    trailingIcon.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      composed: true,
      detail: 1,
      button: 0,
      shiftKey: true,
    }));
    assertEquals(5, events.length);
    assertTrue(events[4] instanceof MouseEvent);
    assertModifiers(events[4], {shiftKey: true});
  });

  test('IconsAndLayoutPerState', async () => {
    const button = element.$.button;
    const style = getComputedStyle(button);
    const label = $$(element, '#label');
    assertTrue(!!label);
    const leadingIcon = $$(element, '#leadingIcon');
    assertTrue(!!leadingIcon);
    const trailingIcon = $$(element, '#trailingIcon');
    assertTrue(!!trailingIcon);

    // Padding and height are the same in every state.
    function assertLayout() {
      assertEquals('8px', style.paddingInlineStart);
      assertEquals('8px', style.paddingInlineEnd);
      assertEquals('8px', style.paddingBlockStart);
      assertEquals('8px', style.paddingBlockEnd);
      assertNear(36, parseFloat(style.height), 0.5);
    }

    const assertTrailingIcon = (className: string) => {
      assertTrue(trailingIcon.classList.contains(className));
      assertEquals('suffix-icon', trailingIcon.getAttribute('slot'));
      assertEquals('true', trailingIcon.getAttribute('aria-hidden'));
      assertFalse(trailingIcon.hasAttribute('tabindex'));
      assertIconShown(trailingIcon);
      assertNear(
          button.getBoundingClientRect().right - 8,
          trailingIcon.getBoundingClientRect().right, 0.1);
    };

    // Exit without X: leading icon, label at rest.
    assertIconShown(leadingIcon);
    assertNear(
        button.getBoundingClientRect().left + 8,
        leadingIcon.getBoundingClientRect().left, 0.1);
    assertIconCollapsed(trailingIcon);
    assertLayout();
    const exitWidth = button.getBoundingClientRect().width;
    const restingLeft = label.getBoundingClientRect().left;

    // Exit with X: close icon; the label slides 18px into the leading icon's
    // space while the button keeps its width.
    await setState(element, AimButtonMode.EXIT, true);
    assertIconCollapsed(leadingIcon);
    assertTrailingIcon('icon-clear');
    assertLayout();
    assertEquals(exitWidth, button.getBoundingClientRect().width);
    assertNear(restingLeft - 18, label.getBoundingClientRect().left, 0.1);

    // Send: arrow, same geometry as exit with X.
    await setState(element, AimButtonMode.SEND, false);
    assertIconCollapsed(leadingIcon);
    assertTrailingIcon('arrow-icon');
    assertLayout();
    assertEquals(exitWidth, button.getBoundingClientRect().width);
    assertNear(restingLeft - 18, label.getBoundingClientRect().left, 0.1);

    // The arrow mirrors and the label shift flips in RTL.
    const html = document.documentElement;
    const originalDir = html.getAttribute('dir');
    try {
      html.dir = 'ltr';
      assertEquals('none', getComputedStyle(trailingIcon).transform);
      html.dir = 'rtl';
      assertEquals(
          'matrix(-1, 0, 0, 1, 0, 0)',
          getComputedStyle(trailingIcon).transform);
      const shiftedRight = label.getBoundingClientRect().right;
      await setState(element, AimButtonMode.EXIT, false);
      assertEquals(exitWidth, button.getBoundingClientRect().width);
      assertNear(shiftedRight - 18, label.getBoundingClientRect().right, 0.1);
    } finally {
      if (originalDir === null) {
        html.removeAttribute('dir');
      } else {
        html.dir = originalDir;
      }
    }

    element.style.setProperty('--aim-motion-duration-short', '100ms');
    element.style.setProperty('--aim-motion-duration-long', '200ms');
    assertEquals('0.2s', getComputedStyle(leadingIcon).transitionDuration);
    assertEquals('0.2s', getComputedStyle(trailingIcon).transitionDuration);
    await setState(element, AimButtonMode.SEND, false);
    assertEquals('0.1s', getComputedStyle(leadingIcon).transitionDuration);
    assertEquals('0.1s', getComputedStyle(trailingIcon).transitionDuration);
  });

  test('DisabledSuppressesClickAndKeyboard', async () => {
    const button = element.$.button;
    const events = recordClicks(element);

    element.disabled = true;
    await element.updateComplete;
    assertTrue(button.disabled);
    assertEquals('0.38', getComputedStyle(element).opacity);
    button.click();
    dispatchKey(button, 'keydown', 'Enter');
    dispatchKey(button, 'keydown', ' ');
    dispatchKey(button, 'keyup', ' ');
    assertEquals(0, events.length);

    element.disabled = false;
    await element.updateComplete;
    button.click();
    assertEquals(1, events.length);
  });

  test('EnergyEffectOnlyInSendWhenEnabled', async () => {
    const button = element.$.button;

    function assertEnergy(expected: boolean) {
      assertEquals(expected, !!$$(element, '#energy'));
      assertEquals(expected, button.noink);
      assertEquals(expected, button.classList.contains('energy-effect'));
    }

    // Flag off: no energy effect in either mode.
    await setState(element, AimButtonMode.EXIT, false);
    assertEnergy(false);
    await setState(element, AimButtonMode.SEND, false);
    assertEnergy(false);

    // Flag on: SEND only, with #energy right after #button for the sibling
    // selectors; EXIT keeps the plain button.
    element.energyEffectAnimationEnabled = true;
    await element.updateComplete;
    assertEnergy(true);
    const energy = $$(element, '#energy');
    assertTrue(!!energy);
    assertEquals(energy, button.nextElementSibling);
    assertEquals(
        'blur(7px)', window.getComputedStyle(energy, '::after').filter);
    await setState(element, AimButtonMode.EXIT, false);
    assertEnergy(false);
    await setState(element, AimButtonMode.EXIT, true);
    assertEnergy(false);
  });
});

suite('ComposeboxAimButtonFocusTest', () => {
  test('FocusOutlineFollowsFocusOutlineVisibleClass', async () => {
    const element = await createButton();
    const outlineColor = 'rgb(1, 2, 3)';
    element.style.setProperty(
        '--color-searchbox-results-icon-focused-outline', outlineColor);
    const reference = document.createElement('cr-button');
    document.body.appendChild(reference);
    await microtasksFinished();
    const button = element.$.button;
    const classList = document.documentElement.classList;
    const hadClass = classList.contains(FOCUS_OUTLINE_CLASS);
    const states: Array<[AimButtonMode, boolean]> = [
      [AimButtonMode.EXIT, false],
      [AimButtonMode.EXIT, true],
      [AimButtonMode.SEND, false],
    ];

    try {
      for (const [mode, showExitIcon] of states) {
        await setState(element, mode, showExitIcon);

        classList.remove(FOCUS_OUTLINE_CLASS);
        reference.focus();
        const referenceOutlineStyle = getComputedStyle(reference).outlineStyle;
        element.focus();
        assertEquals(button, element.shadowRoot.activeElement);
        assertEquals(
            referenceOutlineStyle, getComputedStyle(button).outlineStyle);

        classList.add(FOCUS_OUTLINE_CLASS);
        const style = getComputedStyle(button);
        assertEquals('solid', style.outlineStyle);
        assertEquals('2px', style.outlineWidth);
        assertEquals('2px', style.outlineOffset);
        assertEquals(outlineColor, style.outlineColor);
      }
    } finally {
      classList.toggle(FOCUS_OUTLINE_CLASS, hadClass);
    }
  });
});
