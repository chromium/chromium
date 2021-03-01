// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import 'chrome://settings/lazy_load.js';

/** @fileoverview Suite of tests for settings-textarea. */
suite('SettingsTextarea', function() {
  /** @type {!SettingsTextareaElement} */
  let settingsTextarea;

  /** @type {!HTMLTextAreaElement} */
  let textarea;

  setup(function() {
    PolymerTest.clearBody();
    settingsTextarea = document.createElement('settings-textarea');
    document.body.appendChild(settingsTextarea);
    textarea = settingsTextarea.$.input;
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
    assertEquals('foobar', label.textContent.trim());
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
});
