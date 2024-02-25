// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {downAndUp, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
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

  test('enabled/disabled', () => {
    assertEquals('0', button.getAttribute('tabindex'));
    assertEquals('false', button.getAttribute('aria-disabled'));
    button.disabled = true;
    assertEquals('-1', button.getAttribute('tabindex'));
    assertEquals('true', button.getAttribute('aria-disabled'));
  });

  test('iron-icon is created, reused and removed based on |ironIcon|', () => {
    assertFalse(!!button.shadowRoot!.querySelector('iron-icon'));
    button.ironIcon = 'icon-key';
    assertTrue(!!button.shadowRoot!.querySelector('iron-icon'));
    button.shadowRoot!.querySelector('iron-icon')!.icon = 'icon-key';
    button.ironIcon = 'another-icon-key';
    assertEquals(1, button.shadowRoot!.querySelectorAll('iron-icon').length);
    button.shadowRoot!.querySelector('iron-icon')!.icon = 'another-icon-key';
    button.ironIcon = '';
    assertFalse(!!button.shadowRoot!.querySelector('iron-icon'));
  });

  test('iron-icon children svg and img elements have role set to none', () => {
    button.ironIcon = 'cr:clear';
    assertTrue(!!button.shadowRoot);
    const ironIcons = button.shadowRoot!.querySelectorAll('iron-icon');
    assertEquals(1, ironIcons.length);
    const iconChildren = ironIcons[0]!.shadowRoot!.querySelectorAll('svg, img');
    assertEquals(1, iconChildren.length);
    assertEquals(iconChildren[0]!.getAttribute('role'), 'none');
  });

  test('enter emits click event', async () => {
    const wait = eventToPromise('click', button);
    pressAndReleaseKeyOn(button, -1, [], 'Enter');
    await wait;
  });

  test('space emits click event', async () => {
    const wait = eventToPromise('click', button);
    pressAndReleaseKeyOn(button, -1, [], ' ');
    await wait;
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

  test('when tabindex is -1, it stays -1', async () => {
    document.body.innerHTML =
        getTrustedHTML`<cr-icon-button custom-tab-index="-1"></cr-icon-button>`;
    await flushTasks();
    button = document.body.querySelector('cr-icon-button')!;
    assertEquals('-1', button.getAttribute('tabindex'));
    button.disabled = true;
    assertEquals('-1', button.getAttribute('tabindex'));
    button.disabled = false;
    assertEquals('-1', button.getAttribute('tabindex'));
  });

  test('tabindex update', () => {
    button = document.createElement('cr-icon-button')!;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(button);
    assertEquals('0', button.getAttribute('tabindex'));
    button.customTabIndex = 1;
    assertEquals('1', button.getAttribute('tabindex'));
  });

  test('multiple iron icons', () => {
    button.ironIcon = 'icon1,icon2';
    const elements = button.shadowRoot!.querySelectorAll('iron-icon');
    assertEquals(2, elements.length);
    assertEquals('icon1', elements[0]!.icon);
    assertEquals('icon2', elements[1]!.icon);
  });
});
