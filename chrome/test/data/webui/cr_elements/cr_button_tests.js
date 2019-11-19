// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
// #import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-button', function() {
  let button;

  setup(() => {
    PolymerTest.clearBody();
    button = document.createElement('cr-button');
    document.body.appendChild(button);
  });

  /** @param {string} key */
  function press(key) {
    button.dispatchEvent(new KeyboardEvent('keydown', {key: key}));
    button.dispatchEvent(new KeyboardEvent('keyup', {key: key}));
  }

  test('label is displayed', async () => {
    const widthWithoutLabel = button.offsetWidth;
    document.body.innerHTML = '<cr-button>Long Label</cr-button>';
    button = document.body.querySelector('cr-button');
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
    const clickHandler = () => {
      clickCount++;
    };
    button.addEventListener('click', clickHandler);

    const checkClicks = expectedCount => {
      clickCount = 0;
      press('Enter');
      press(' ');
      button.dispatchEvent(new MouseEvent('click'));
      button.click();
      assertEquals(expectedCount, clickCount);
    };

    checkClicks(4);
    button.disabled = true;
    checkClicks(0);
    button.disabled = false;
    checkClicks(4);

    button.removeEventListener('click', clickHandler);
  });

  test('when tabindex is -1, it stays -1', async () => {
    document.body.innerHTML = '<cr-button tabindex="-1"></cr-button>';
    button = document.body.querySelector('cr-button');
    assertEquals('-1', button.getAttribute('tabindex'));
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

  test('tap event is emitted on click', async () => {
    const wait = test_util.eventToPromise('tap', button);
    button.click();
    await wait;
  });
});
