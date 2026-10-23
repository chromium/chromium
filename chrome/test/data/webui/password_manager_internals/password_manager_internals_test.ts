// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';

suite('PasswordManagerInternals', function() {
  test('LogText', () => {
    const divLogs = document.getElementById('log-entries');
    assertTrue(!!divLogs);
    assertNotEquals(null, divLogs, 'The <div> with logs not found.');
    assertNotEquals(
        null, divLogs.innerHTML.match(/text for testing/),
        'The logged text not found.');
    assertEquals(
        null, divLogs.innerHTML.match(/<script>/),
        'The logged text was not escaped.');
  });

  test('LogEmpty', () => {
    const divLogs = document.getElementById('log-entries');
    assertTrue(!!divLogs);
    assertNotEquals(null, divLogs, 'The <div> with logs not found.');
    assertEquals(
        null, divLogs.innerHTML.match(/[^\s]/),
        'There were some logs:' + divLogs.innerHTML);
  });

  test('NonIncognitoDescription', () => {
    const body = document.getElementsByTagName('body')[0];
    assertTrue(!!body);
    const bodyText = body.innerText;
    let match = bodyText.match(/logs are listed below/);
    assertTrue(!!match);
    assertEquals(1, match.length, 'Where are the logs in: ' + bodyText);
    match = bodyText.match(/in Incognito/);
    assertEquals(null, match);
  });

  test('IncognitoDescription', () => {
    const body = document.getElementsByTagName('body')[0];
    assertTrue(!!body);
    const bodyText = body.innerText;
    let match = bodyText.match(/in Incognito/);
    assertTrue(!!match);
    assertEquals(1, match.length, 'Where is Incognito in: ' + bodyText);
    match = bodyText.match(/logs are listed below/);
    assertEquals(null, match);
  });

  test('AutoScroll', () => {
    document.documentElement.style.scrollBehavior = 'auto';
    const checkbox =
        document.querySelector<HTMLInputElement>('#EnableAutoscroll');
    assertTrue(!!checkbox);
    assertTrue(checkbox.checked);
    assertTrue(document.body.offsetHeight > window.innerHeight);

    const markerButton = document.getElementById('marker-fake-button');
    assertTrue(!!markerButton);
    markerButton.click();

    assertTrue(window.scrollY > 0);
    assertTrue(
        Math.ceil(window.innerHeight + window.scrollY) >=
        document.body.offsetHeight);

    const scrollBeforeMarker = window.scrollY;
    markerButton.click();
    assertTrue(window.scrollY > scrollBeforeMarker);
    assertTrue(
        Math.ceil(window.innerHeight + window.scrollY) >=
        document.body.offsetHeight);

    checkbox.click();
    window.scrollTo(0, 0);
    assertEquals(0, window.scrollY);

    checkbox.click();
    assertTrue(checkbox.checked);
    assertTrue(
        Math.ceil(window.innerHeight + window.scrollY) >=
        document.body.offsetHeight);
  });

  test('CursorWiggle', () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    try {
      assertFalse(document.body.classList.contains('password-manager-cursor'));

      const moveCursor = (x: number, y: number) => {
        document.dispatchEvent(
            new MouseEvent('mousemove', {clientX: x, clientY: y}));
      };

      // Moving in a single direction should not trigger the custom cursor.
      moveCursor(10, 10);
      moveCursor(50, 10);
      moveCursor(90, 10);
      moveCursor(130, 10);
      assertFalse(document.body.classList.contains('password-manager-cursor'));

      // Wiggling back and forth should trigger the password manager logo
      // cursor.
      moveCursor(50, 10);
      moveCursor(130, 10);
      moveCursor(50, 10);
      assertTrue(document.body.classList.contains('password-manager-cursor'));
      assertTrue(window.getComputedStyle(document.body)
                     .cursor.includes('password_manager_logo.svg'));

      // Cursor should revert after the 1000ms timeout elapses.
      mockTimer.tick(999);
      assertTrue(document.body.classList.contains('password-manager-cursor'));
      mockTimer.tick(1);
      assertFalse(document.body.classList.contains('password-manager-cursor'));
    } finally {
      mockTimer.uninstall();
    }
  });
});
