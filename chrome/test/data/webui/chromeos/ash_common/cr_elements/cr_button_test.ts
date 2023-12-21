// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  test('label is displayed', async () => {
    const widthWithoutLabel = button.offsetWidth;
    document.body.innerHTML = getTrustedHTML`<cr-button>Long Label</cr-button>`;
    button = document.body.querySelector('cr-button')!;
    assertTrue(widthWithoutLabel < button.offsetWidth);
  });

  test('tabindex and aria-disabled', () => {
    assertFalse(button.disabled);
    assertFalse(button.hasAttribute('disabled'));
    assertEquals('0', button.getAttribute('tabindex'));
    assertEquals('false', button.getAttribute('aria-disabled'));
    button.disabled = true;
    assertTrue(button.hasAttribute('disabled'));
    assertEquals('-1', button.getAttribute('tabindex'));
    assertEquals('true', button.getAttribute('aria-disabled'));
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
    checkClicks(0);
    button.disabled = false;
    checkClicks(4);

    button.removeEventListener('click', clickHandler);
  });

  test('when tabindex is -1, it stays -1', async () => {
    document.body.innerHTML =
        getTrustedHTML`<cr-button custom-tab-index="-1"></cr-button>`;
    button = document.body.querySelector('cr-button')!;
    assertEquals('-1', button.getAttribute('tabindex'));
    button.disabled = true;
    assertEquals('-1', button.getAttribute('tabindex'));
    button.disabled = false;
    assertEquals('-1', button.getAttribute('tabindex'));
  });

  test('tabindex update', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    button = document.createElement('cr-button');
    document.body.appendChild(button);
    assertEquals('0', button.getAttribute('tabindex'));
    button.customTabIndex = 1;
    assertEquals('1', button.getAttribute('tabindex'));
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
    assertEquals('8px', buttonStyle.padding);

    document.documentElement.toggleAttribute('chrome-refresh-2023', true);
    assertEquals('8px 16px 8px 12px', buttonStyle.padding);
    document.documentElement.removeAttribute('chrome-refresh-2023');

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
    assertEquals('8px', buttonStyle.padding);

    document.documentElement.toggleAttribute('chrome-refresh-2023', true);
    assertEquals('8px 12px 8px 16px', buttonStyle.padding);
    document.documentElement.removeAttribute('chrome-refresh-2023');
  });
});
