// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_textarea/cr_textarea.js';

import {CrTextareaElement} from 'chrome://resources/ash/common/cr_elements/cr_textarea/cr_textarea.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('cr-textarea-focus-test', function() {
  let crTextarea: CrTextareaElement;
  let textarea: HTMLTextAreaElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crTextarea = document.createElement('cr-textarea');
    document.body.appendChild(crTextarea);
    textarea = crTextarea.$.input;
    flush();
  });

  test('propertyBindings', function() {
    assertFalse(textarea.autofocus);
    crTextarea.setAttribute('autofocus', 'autofocus');
    assertTrue(textarea.autofocus);
  });

  test('valueSetCorrectly', function() {
    crTextarea.value = 'hello';
    assertEquals(crTextarea.value, textarea.value);

    // |value| is copied when typing triggers inputEvent.
    textarea.value = 'hello sir';
    textarea.dispatchEvent(new InputEvent('input'));
    assertEquals(crTextarea.value, textarea.value);
  });

  test('labelHiddenWhenEmpty', function() {
    const label = crTextarea.$.label;
    assertTrue(label.hidden);
    crTextarea.label = 'foobar';
    assertFalse(label.hidden);
    assertEquals('foobar', label.textContent!.trim());
    assertEquals('foobar', textarea.getAttribute('aria-label'));
  });

  test('disabledSetCorrectly', function() {
    assertFalse(textarea.disabled);
    assertFalse(textarea.hasAttribute('disabled'));
    assertFalse(crTextarea.hasAttribute('disabled'));
    assertEquals('false', crTextarea.getAttribute('aria-disabled'));
    crTextarea.disabled = true;
    assertTrue(textarea.disabled);
    assertTrue(textarea.hasAttribute('disabled'));
    assertTrue(crTextarea.hasAttribute('disabled'));
    assertEquals('true', crTextarea.getAttribute('aria-disabled'));
  });

  test('rowsSetCorrectly', function() {
    const kDefaultRows = crTextarea.rows;
    const kNewRows = 42;
    assertEquals(kDefaultRows, textarea.rows);
    crTextarea.rows = kNewRows;
    assertEquals(kNewRows, textarea.rows);
  });

  test('underlineAndFooterColorsWhenFocused', async function() {
    const firstFooter = crTextarea.$.firstFooter;
    const underline = crTextarea.$.underline;

    const whenTransitionEnd = eventToPromise('transitionend', underline);
    crTextarea.firstFooter = 'first footer';
    flush();

    assertEquals('0', getComputedStyle(underline).opacity);
    const currentColor = getComputedStyle(firstFooter).color;

    crTextarea.$.input.focus();
    flush();

    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertEquals(currentColor, getComputedStyle(firstFooter).color);
    });
  });

  test('underlineAndFooterColorsWhenInvalid', function() {
    const firstFooter = crTextarea.$.firstFooter;
    const underline = crTextarea.$.underline;

    const whenTransitionEnd = eventToPromise('transitionend', underline);
    crTextarea.firstFooter = 'first footer';
    flush();

    assertEquals('0', getComputedStyle(underline).opacity);
    const currentColor = getComputedStyle(firstFooter).color;

    crTextarea.invalid = true;
    flush();

    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertNotEquals(currentColor, getComputedStyle(firstFooter).color);
    });
  });
});
