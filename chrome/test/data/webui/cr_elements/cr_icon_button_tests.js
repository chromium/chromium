// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
// #import 'chrome://resources/cr_elements/icons.m.js';

// #import {downAndUp, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {eventToPromise, flushTasks} from '../test_util.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

suite('cr-icon-button', function() {
  /** @type {!CrIconButtonElement} */
  let button;

  /** @override */
  suiteSetup(function() {
    /* #ignore */ return PolymerTest.importHtml(
        /* #ignore */ 'chrome://resources/cr_elements/icons.html');
  });

  /** @param {string} key */
  function press(key) {
    button.dispatchEvent(new KeyboardEvent('keydown', {key}));
    button.dispatchEvent(new KeyboardEvent('keyup', {key}));
  }

  setup(async () => {
    document.body.innerHTML = '';
    button = /** @type {!CrIconButtonElement} */ (
        document.createElement('cr-icon-button'));
    document.body.appendChild(button);
    await test_util.flushTasks();
  });

  test('enabled/disabled', () => {
    assertEquals('0', button.getAttribute('tabindex'));
    assertEquals('false', button.getAttribute('aria-disabled'));
    button.disabled = true;
    assertEquals('-1', button.getAttribute('tabindex'));
    assertEquals('true', button.getAttribute('aria-disabled'));
  });

  test('iron-icon is created, reused and removed based on |ironIcon|', () => {
    assertFalse(!!button.$$('iron-icon'));
    button.ironIcon = 'icon-key';
    assertTrue(!!button.$$('iron-icon'));
    button.$$('iron-icon').icon = 'icon-key';
    button.ironIcon = 'another-icon-key';
    assertEquals(1, button.shadowRoot.querySelectorAll('iron-icon').length);
    button.$$('iron-icon').icon = 'another-icon-key';
    button.ironIcon = '';
    assertFalse(!!button.$$('iron-icon'));
  });

  test('iron-icon children svg and img elements have role set to none', () => {
    button.ironIcon = 'cr:clear';
    assertTrue(!!button.shadowRoot);
    const ironIcons = button.shadowRoot.querySelectorAll('iron-icon');
    assertEquals(1, ironIcons.length);
    const iconChildren = ironIcons[0].shadowRoot.querySelectorAll('svg', 'img');
    assertEquals(1, iconChildren.length);
    assertEquals(iconChildren[0].getAttribute('role'), 'none');
  });

  test('enter emits click event', async () => {
    const wait = test_util.eventToPromise('click', button);
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], 'Enter');
    await wait;
  });

  test('space emits click event', async () => {
    const wait = test_util.eventToPromise('click', button);
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], ' ');
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
    await test_util.flushTasks();
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], 'Enter');
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], ' ');
    MockInteractions.downAndUp(button);
    button.click();
    await test_util.flushTasks();
    assertEquals(0, clickCount);

    button.disabled = false;
    await test_util.flushTasks();
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], 'Enter');
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], ' ');
    MockInteractions.downAndUp(button);
    button.click();
    await test_util.flushTasks();
    assertEquals(4, clickCount);
    button.removeEventListener('click', clickHandler);
  });

  test('when tabindex is -1, it stays -1', async () => {
    document.body.innerHTML =
        '<cr-icon-button custom-tab-index="-1"></cr-icon-button>';
    await test_util.flushTasks();
    button = /** @type {!CrIconButtonElement} */ (
        document.body.querySelector('cr-icon-button'));
    assertEquals('-1', button.getAttribute('tabindex'));
    button.disabled = true;
    assertEquals('-1', button.getAttribute('tabindex'));
    button.disabled = false;
    assertEquals('-1', button.getAttribute('tabindex'));
  });

  test('tabindex update', async () => {
    document.body.innerHTML = '<cr-icon-button></cr-icon-button>';
    button = /** @type {!CrIconButtonElement} */ (
        document.body.querySelector('cr-icon-button'));
    assertEquals('0', button.getAttribute('tabindex'));
    button.customTabIndex = 1;
    assertEquals('1', button.getAttribute('tabindex'));
  });

  test('ripple is a circle with background icon or single iron-icon', () => {
    const ripple = button.getRipple();
    assertEquals(undefined, button.ironIcon);
    assertTrue(ripple.classList.contains('circle'));
    button.ironIcon = 'icon';
    assertTrue(ripple.classList.contains('circle'));
    button.ironIcon = 'icon,icon';
    assertFalse(ripple.classList.contains('circle'));
  });

  test('multiple iron icons', () => {
    button.ironIcon = 'icon1,icon2';
    const elements = button.shadowRoot.querySelectorAll('iron-icon');
    assertEquals(2, elements.length);
    assertEquals('icon1', elements[0].icon);
    assertEquals('icon2', elements[1].icon);
  });
});
