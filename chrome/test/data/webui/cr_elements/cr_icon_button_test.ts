// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {downAndUp, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

// clang-format on

suite('cr-icon-button', function() {
  let button: CrIconButtonElement;

  function press(key: string) {
    button.dispatchEvent(new KeyboardEvent('keydown', {key}));
    button.dispatchEvent(new KeyboardEvent('keyup', {key}));
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    button = document.createElement('cr-icon-button');
    document.body.appendChild(button);
    await flushTasks();
  });

  test('enabled/disabled', async () => {
    assertFalse(button.disabled);
    assertFalse(button.hasAttribute('disabled'));
    assertEquals('0', button.getAttribute('tabindex'));
    assertEquals('false', button.getAttribute('aria-disabled'));
    button.disabled = true;
    await button.updateComplete;
    assertTrue(button.hasAttribute('disabled'));
    assertEquals('-1', button.getAttribute('tabindex'));
    assertEquals('true', button.getAttribute('aria-disabled'));
  });

  // This test documents previously undefined behavior of cr-icon-button when a
  // 'tabindex' attribute is set by the parent, which seems to be actually
  // relied upon by cr-icon-button client code. The behavior below should
  // possibly be improved to preserve the original tabindex upon re-enabling.
  test('external tabindex', async () => {
    document.body.innerHTML =
        getTrustedHTML`<cr-icon-button tabindex="10"></cr-icon-button>`;
    button = document.body.querySelector('cr-icon-button')!;

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

  test('cr-icon created, reused, removed based on |ironIcon|', async () => {
    assertFalse(!!button.shadowRoot!.querySelector('cr-icon'));
    button.ironIcon = 'icon-key';
    await button.updateComplete;

    assertTrue(!!button.shadowRoot!.querySelector('cr-icon'));
    button.shadowRoot!.querySelector('cr-icon')!.icon = 'icon-key';
    button.ironIcon = 'another-icon-key';
    await button.updateComplete;

    assertEquals(1, button.shadowRoot!.querySelectorAll('cr-icon').length);
    button.shadowRoot!.querySelector('cr-icon')!.icon = 'another-icon-key';
    button.ironIcon = '';
    await button.updateComplete;

    assertFalse(!!button.shadowRoot!.querySelector('cr-icon'));
  });

  test('cr-icon children svg and img elements role set to none', async () => {
    button.ironIcon = 'cr:clear';
    await microtasksFinished();
    assertTrue(!!button.shadowRoot);
    const ironIcons = button.shadowRoot!.querySelectorAll('cr-icon');
    assertEquals(1, ironIcons.length);
    const iconChildren = ironIcons[0]!.shadowRoot!.querySelectorAll('svg, img');
    assertEquals(1, iconChildren.length);
    assertEquals(iconChildren[0]!.getAttribute('role'), 'none');
  });

  test('enter emits click event', () => {
    const wait = eventToPromise('click', button);
    pressAndReleaseKeyOn(button, -1, [], 'Enter');
    return wait;
  });

  test('space emits click event', () => {
    const wait = eventToPromise('click', button);
    pressAndReleaseKeyOn(button, -1, [], ' ');
    return wait;
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

  test('disabled prevents UI and programmatic clicks', async () => {
    let clickCount = 0;
    const clickHandler = () => {
      clickCount++;
    };
    button.addEventListener('click', clickHandler);

    button.disabled = true;
    await flushTasks();
    pressAndReleaseKeyOn(button, -1, [], 'Enter');
    pressAndReleaseKeyOn(button, -1, [], ' ');
    downAndUp(button);
    button.click();
    await flushTasks();
    assertEquals(0, clickCount);

    button.disabled = false;
    await flushTasks();
    pressAndReleaseKeyOn(button, -1, [], 'Enter');
    pressAndReleaseKeyOn(button, -1, [], ' ');
    downAndUp(button);
    button.click();
    await flushTasks();
    assertEquals(4, clickCount);
    button.removeEventListener('click', clickHandler);
  });

  test('multiple iron icons', async () => {
    button.ironIcon = 'icon1,icon2';
    await button.updateComplete;
    const elements = button.shadowRoot!.querySelectorAll('cr-icon');
    assertEquals(2, elements.length);
    assertEquals('icon1', elements[0]!.icon);
    assertEquals('icon2', elements[1]!.icon);
  });
});
