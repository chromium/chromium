// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {SettingsCheckboxListEntryElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SettingsCheckboxListEntry', function() {
  let checkboxListEntry: SettingsCheckboxListEntryElement;

  setup(function() {
    checkboxListEntry = document.createElement('settings-checkbox-list-entry');
    document.body.appendChild(checkboxListEntry);
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('click', function() {
    assertFalse(checkboxListEntry.checked);

    checkboxListEntry.click();
    assertTrue(checkboxListEntry.checked);

    checkboxListEntry.click();
    assertFalse(checkboxListEntry.checked);
  });

  test('space', function() {
    assertFalse(checkboxListEntry.checked);

    checkboxListEntry.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    assertFalse(checkboxListEntry.checked);
    checkboxListEntry.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
    assertTrue(checkboxListEntry.checked);

    checkboxListEntry.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    assertTrue(checkboxListEntry.checked);
    checkboxListEntry.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
    assertFalse(checkboxListEntry.checked);
  });

  test('enter', function() {
    assertFalse(checkboxListEntry.checked);

    checkboxListEntry.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    assertTrue(checkboxListEntry.checked);
    checkboxListEntry.dispatchEvent(new KeyboardEvent('keyup', {key: 'Enter'}));
    assertTrue(checkboxListEntry.checked);

    checkboxListEntry.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    assertFalse(checkboxListEntry.checked);
    checkboxListEntry.dispatchEvent(new KeyboardEvent('keyup', {key: 'Enter'}));
    assertFalse(checkboxListEntry.checked);
  });

  test('checked changed', function() {
    assertFalse(checkboxListEntry.checked);
    assertFalse(checkboxListEntry.$.checkbox.checked);
    assertEquals('false', checkboxListEntry.getAttribute('aria-checked'));

    checkboxListEntry.checked = true;
    assertTrue(checkboxListEntry.$.checkbox.checked);
    assertEquals('true', checkboxListEntry.getAttribute('aria-checked'));
  });

  test('tabindex changed', function() {
    assertEquals('0', checkboxListEntry.getAttribute('tabindex'));
    assertEquals('false', checkboxListEntry.getAttribute('aria-hidden'));

    checkboxListEntry.setAttribute('tabindex', '-1');
    assertEquals('true', checkboxListEntry.getAttribute('aria-hidden'));
  });
});
