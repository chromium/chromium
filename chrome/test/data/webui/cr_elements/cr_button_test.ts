// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-button', function() {
  let button: CrButtonElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    button =
        /** @type {!CrButtonElement} */ (document.createElement('cr-button'));
    document.body.appendChild(button);
  });

  function press(key: string) {
    button.dispatchEvent(new KeyboardEvent('keydown', {key}));
    button.dispatchEvent(new KeyboardEvent('keyup', {key}));
  }

  test('label is displayed', () => {
    const widthWithoutLabel = button.offsetWidth;
    document.body.innerHTML = getTrustedHTML`<cr-button>Long Label</cr-button>`;
    button = document.body.querySelector('cr-button')!;
    assertTrue(widthWithoutLabel < button.offsetWidth);
  });

  test('tabindex and aria-disabled', async () => {
    assertFalse(button.disabled);
    assertFalse(button.hasAttribute('disabled'));
    assertEquals('0', button.getAttribute('tabindex'));
    assertEquals('false', button.getAttribute('aria-disabled'));
    button.disabled = true;
    await microtasksFinished();
    assertTrue(button.hasAttribute('disabled'));
    assertEquals('-1', button.getAttribute('tabindex'));
    assertEquals('true', button.getAttribute('aria-disabled'));
  });

  // This test documents previously undefined behavior of cr-button when a
  // 'tabindex' attribute is set by the parent, which seems to be actually
  // relied upon by cr-button client code. The behavior below should possibly be
  // improved to preserve the original tabindex upon re-enabling.
  test('external tabindex', async () => {
    document.body.innerHTML =
        getTrustedHTML`<cr-button tabindex="10"></cr-button>`;
    button = document.body.querySelector('cr-button')!;

    // Check that initial tabindex value is preserved post-initialization.
    assertFalse(button.disabled);
    assertEquals('10', button.getAttribute('tabindex'));

    // Check that tabindex updates when disabled.
    button.disabled = true;
    await microtasksFinished();
    assertEquals('-1', button.getAttribute('tabindex'));

    // Check that tabindex resets to 0 and not the initial value after
    // re-enabling.
    button.disabled = false;
    await microtasksFinished();
    assertEquals('0', button.getAttribute('tabindex'));
  });

  test('enter/space/click events and programmatic click() calls', async () => {
    let clickCount = 0;
    function clickHandler() {
      clickCount++;
    }
    button.addEventListener('click', clickHandler);

    function checkClicks(expectedCount: number) {
      clickCount = 0;
      press('Enter');
      press(' ');
      button.dispatchEvent(new MouseEvent('click'));
      button.click();
      assertEquals(expectedCount, clickCount);
    }

    checkClicks(4);
    button.disabled = true;
    await microtasksFinished();
    checkClicks(0);
    button.disabled = false;
    await microtasksFinished();
    checkClicks(4);

    button.removeEventListener('click', clickHandler);
  });

  test('hidden', () => {
    assertFalse(button.hidden);
    assertFalse(button.hasAttribute('hidden'));
    assertNotEquals('none', getComputedStyle(button).display);
    button.hidden = true;
    assertTrue(button.hasAttribute('hidden'));
    assertEquals('none', getComputedStyle(button).display);
    button.hidden = false;
    assertFalse(button.hasAttribute('hidden'));
    assertNotEquals('none', getComputedStyle(button).display);
  });

  test('space up does not click without space down', () => {
    let clicked = false;
    button.addEventListener('click', () => {
      clicked = true;
    }, {once: true});
    button.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
    assertFalse(clicked);
    press(' ');
    assertTrue(clicked);
  });

  test('space up events will not result in one click if loses focus', () => {
    let clicked = false;
    button.addEventListener('click', () => {
      clicked = true;
    }, {once: true});
    button.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    button.dispatchEvent(new Event('blur'));
    button.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
    assertFalse(clicked);
    press(' ');
    assertTrue(clicked);
  });

  test('UpdatesStyleWithIcons', async () => {
    const buttonStyle = window.getComputedStyle(button);
    const whenPrefixSlotchange =
        eventToPromise('slotchange', button.$.prefixIcon);
    const icon = document.createElement('div');
    icon.slot = 'prefix-icon';
    button.appendChild(icon);

    const text = document.createTextNode('Hello world');
    button.appendChild(text);
    await whenPrefixSlotchange;

    assertEquals('8px', buttonStyle.gap);
    assertEquals('8px 16px 8px 12px', buttonStyle.padding);

    const whenPrefixSlotRemoved =
        eventToPromise('slotchange', button.$.prefixIcon);
    icon.remove();
    await whenPrefixSlotRemoved;

    assertEquals('normal', buttonStyle.gap);
    assertEquals('8px 16px', buttonStyle.padding);

    const whenSuffixSlotchange =
        eventToPromise('slotchange', button.$.suffixIcon);
    icon.slot = 'suffix-icon';
    button.appendChild(icon);
    await whenSuffixSlotchange;

    assertEquals('8px', buttonStyle.gap);
    assertEquals('8px 12px 8px 16px', buttonStyle.padding);
  });
});
