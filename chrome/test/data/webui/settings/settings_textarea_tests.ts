// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsTextareaElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @fileoverview Suite of tests for settings-textarea. */
suite('SettingsTextarea', function() {
  let settingsTextarea: SettingsTextareaElement;
  let textarea: HTMLTextAreaElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsTextarea = document.createElement('settings-textarea');
    document.body.appendChild(settingsTextarea);
    textarea = settingsTextarea.$.input;
    flush();
  });

  test('propertyBindings', function() {
    assertFalse(textarea.autofocus);
    settingsTextarea.setAttribute('autofocus', 'autofocus');
    assertTrue(textarea.autofocus);
  });

  test('valueSetCorrectly', function() {
    settingsTextarea.value = 'hello';
    assertEquals(settingsTextarea.value, textarea.value);

    // |value| is copied when typing triggers inputEvent.
    textarea.value = 'hello sir';
    textarea.dispatchEvent(new InputEvent('input'));
    assertEquals(settingsTextarea.value, textarea.value);
  });

  test('labelHiddenWhenEmpty', function() {
    const label = settingsTextarea.$.label;
    assertTrue(label.hidden);
    settingsTextarea.label = 'foobar';
    assertFalse(label.hidden);
    assertEquals('foobar', label.textContent!.trim());
    assertEquals('foobar', textarea.getAttribute('aria-label'));
  });

  test('disabledSetCorrectly', function() {
    assertFalse(textarea.disabled);
    assertFalse(textarea.hasAttribute('disabled'));
    assertFalse(settingsTextarea.hasAttribute('disabled'));
    assertEquals('false', settingsTextarea.getAttribute('aria-disabled'));
    settingsTextarea.disabled = true;
    assertTrue(textarea.disabled);
    assertTrue(textarea.hasAttribute('disabled'));
    assertTrue(settingsTextarea.hasAttribute('disabled'));
    assertEquals('true', settingsTextarea.getAttribute('aria-disabled'));
  });

  test('rowsSetCorrectly', function() {
    const kDefaultRows = settingsTextarea.rows;
    const kNewRows = 42;
    assertEquals(kDefaultRows, textarea.rows);
    settingsTextarea.rows = kNewRows;
    assertEquals(kNewRows, textarea.rows);
  });

  test('underlineAndFooterColorsWhenFocused', async function() {
    const firstFooter = settingsTextarea.$.firstFooter;
    const underline = settingsTextarea.$.underline;

    const whenTransitionEnd = eventToPromise('transitionend', underline);
    settingsTextarea.firstFooter = 'first footer';
    flush();

    assertEquals('0', getComputedStyle(underline).opacity);
    const currentColor = getComputedStyle(firstFooter).color;

    settingsTextarea.$.input.focus();
    flush();

    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertEquals(currentColor, getComputedStyle(firstFooter).color);
    });
  });

  test('underlineAndFooterColorsWhenInvalid', function() {
    const firstFooter = settingsTextarea.$.firstFooter;
    const underline = settingsTextarea.$.underline;

    const whenTransitionEnd = eventToPromise('transitionend', underline);
    settingsTextarea.firstFooter = 'first footer';
    flush();

    assertEquals('0', getComputedStyle(underline).opacity);
    const currentColor = getComputedStyle(firstFooter).color;

    settingsTextarea.invalid = true;
    flush();

    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertNotEquals(currentColor, getComputedStyle(firstFooter).color);
    });
  });
});
