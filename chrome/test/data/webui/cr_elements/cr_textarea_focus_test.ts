// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-textarea-focus-test', function() {
  let crTextarea: CrTextareaElement;
  let textarea: HTMLTextAreaElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crTextarea = document.createElement('cr-textarea');
    document.body.appendChild(crTextarea);
    textarea = crTextarea.$.input;
    return microtasksFinished();
  });

  test('propertyBindings', async () => {
    assertFalse(textarea.autofocus);
    crTextarea.setAttribute('autofocus', 'autofocus');
    await microtasksFinished();
    assertTrue(textarea.autofocus);
  });

  test('valueSetCorrectly', async () => {
    crTextarea.value = 'hello';
    await microtasksFinished();
    assertEquals(crTextarea.value, textarea.value);

    // |value| is copied when typing triggers inputEvent.
    textarea.value = 'hello sir';
    textarea.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();
    assertEquals(crTextarea.value, textarea.value);
  });

  test('labelHiddenWhenEmpty', async () => {
    const label = crTextarea.$.label;
    assertTrue(label.hidden);
    crTextarea.label = 'foobar';
    await microtasksFinished();
    assertFalse(label.hidden);
    assertEquals('foobar', label.textContent.trim());
    assertEquals('foobar', textarea.getAttribute('aria-label'));
  });

  test('disabledSetCorrectly', async () => {
    assertFalse(textarea.disabled);
    assertFalse(textarea.hasAttribute('disabled'));
    assertFalse(crTextarea.hasAttribute('disabled'));
    assertEquals('false', crTextarea.getAttribute('aria-disabled'));
    crTextarea.disabled = true;
    await microtasksFinished();
    assertTrue(textarea.disabled);
    assertTrue(textarea.hasAttribute('disabled'));
    assertTrue(crTextarea.hasAttribute('disabled'));
    assertEquals('true', crTextarea.getAttribute('aria-disabled'));
  });

  test('rowsSetCorrectly', async () => {
    const kDefaultRows = crTextarea.rows;
    const kNewRows = 42;
    assertEquals(kDefaultRows, textarea.rows);
    crTextarea.rows = kNewRows;
    await microtasksFinished();
    assertEquals(kNewRows, textarea.rows);
  });

  test('underlineAndFooterColorsWhenFocused', async function() {
    const firstFooter = crTextarea.$.firstFooter;
    const underline = crTextarea.$.underline;

    const whenTransitionEnd = eventToPromise('transitionend', underline);
    crTextarea.firstFooter = 'first footer';
    await microtasksFinished();

    assertEquals('0', getComputedStyle(underline).opacity);
    const currentColor = getComputedStyle(firstFooter).color;

    crTextarea.$.input.focus();

    await whenTransitionEnd;
    assertEquals('1', getComputedStyle(underline).opacity);
    assertEquals(currentColor, getComputedStyle(firstFooter).color);
  });

  test('underlineAndFooterColorsWhenInvalid', async () => {
    const firstFooter = crTextarea.$.firstFooter;
    const underline = crTextarea.$.underline;

    const whenTransitionEnd = eventToPromise('transitionend', underline);
    crTextarea.firstFooter = 'first footer';
    await microtasksFinished();

    assertEquals('0', getComputedStyle(underline).opacity);
    const currentColor = getComputedStyle(firstFooter).color;

    crTextarea.invalid = true;

    await whenTransitionEnd;
    assertEquals('1', getComputedStyle(underline).opacity);
    assertNotEquals(currentColor, getComputedStyle(firstFooter).color);
  });

  test('autogrows', async () => {
    let currentHeight = crTextarea.$.input.offsetHeight;
    crTextarea.autogrow = true;
    await microtasksFinished();

    crTextarea.rows = 1;
    await microtasksFinished();
    assertTrue(
        currentHeight > crTextarea.$.input.offsetHeight,
        'decreasing rows decreases the min-height');
    currentHeight = crTextarea.$.input.offsetHeight;

    crTextarea.value = '\n\n\n\n\n';
    await microtasksFinished();
    assertTrue(
        currentHeight < crTextarea.$.input.offsetHeight,
        'textarea grows in height');
    currentHeight = crTextarea.$.input.offsetHeight;

    crTextarea.value = '\n\n\n\n\n\n\n';
    await microtasksFinished();
    assertTrue(
        currentHeight < crTextarea.$.input.offsetHeight,
        'textarea continues to grow');

    const textareaStyle = crTextarea.$.input.computedStyleMap();
    const paddingBlock =
        (textareaStyle.get('padding-top') as CSSUnitValue).value +
        (textareaStyle.get('padding-bottom') as CSSUnitValue).value;
    crTextarea.style.setProperty('--cr-textarea-autogrow-max-height', '60px');
    assertEquals(
        60 + paddingBlock, crTextarea.$.input.offsetHeight,
        'sets the max-height');
  });
});
