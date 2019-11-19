// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise, whenAttributeIs} from '../test_util.m.js';
// clang-format on

suite('cr-input', function() {
  let crInput;
  let input;

  setup(function() {
    regenerateNewInput();
  });

  function regenerateNewInput() {
    PolymerTest.clearBody();
    crInput = document.createElement('cr-input');
    document.body.appendChild(crInput);
    input = crInput.$.input;
    Polymer.dom.flush();
  }

  test('AttributesCorrectlySupported', function() {
    const attributesToTest = [
      // [externalName, internalName, defaultValue, testValue]
      ['autofocus', 'autofocus', false, true],
      ['disabled', 'disabled', false, true],
      ['max', 'max', '', '100'],
      ['min', 'min', '', '1'],
      ['maxlength', 'maxLength', -1, 5],
      ['minlength', 'minLength', -1, 5],
      ['pattern', 'pattern', '', '[a-z]+'],
      ['readonly', 'readOnly', false, true],
      ['required', 'required', false, true],
      ['tabindex', 'tabIndex', 0, -1],
      ['type', 'type', 'text', 'password'],
    ];

    attributesToTest.forEach(attr => {
      regenerateNewInput();
      assertEquals(attr[2], input[attr[1]]);
      crInput.setAttribute(attr[0], attr[3]);
      assertEquals(attr[3], input[attr[1]]);
    });
  });

  test('UnsupportedTypeThrows', function() {
    assertThrows(function() {
      crInput.type = 'checkbox';
    });
  });

  test('togglingDisableModifiesTabIndexCorrectly', function() {
    // Do innerHTML instead of createElement to make sure it's correct right
    // after being attached, and not messed up by disabledChanged_.
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-input tabindex="14"></cr-input>
    `;
    crInput = document.querySelector('cr-input');
    input = crInput.$.input;
    Polymer.dom.flush();

    assertEquals('14', crInput.getAttribute('tabindex'));
    assertEquals(14, input.tabIndex);
    crInput.disabled = true;
    assertEquals(null, crInput.getAttribute('tabindex'));
    assertEquals(true, input.disabled);
    crInput.disabled = false;
    assertEquals('14', crInput.getAttribute('tabindex'));
    assertEquals(14, input.tabIndex);
  });

  test('startingWithDisableSetsTabIndexCorrectly', function() {
    // Makes sure tabindex is recorded even if cr-input starts as disabled
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-input tabindex="14" disabled></cr-input>
    `;
    crInput = document.querySelector('cr-input');
    input = crInput.$.input;
    Polymer.dom.flush();

    return test_util.whenAttributeIs(input, 'tabindex', null).then(() => {
      assertEquals(null, crInput.getAttribute('tabindex'));
      assertEquals(true, input.disabled);
      crInput.disabled = false;
      assertEquals('14', crInput.getAttribute('tabindex'));
      assertEquals(14, input.tabIndex);
    });
  });

  test('pointerDownAndTabIndex', function() {
    crInput.fire('pointerdown');
    assertEquals(null, crInput.getAttribute('tabindex'));
    return test_util.whenAttributeIs(crInput, 'tabindex', '0').then(() => {
      assertEquals(0, input.tabIndex);
    });
  });

  test('pointerdownWhileDisabled', function(done) {
    // pointerdown while disabled doesn't mess with tabindex.
    crInput.tabindex = 14;
    crInput.disabled = true;
    assertEquals(null, crInput.getAttribute('tabindex'));
    assertEquals(true, input.disabled);
    crInput.fire('pointerdown');
    assertEquals(null, crInput.getAttribute('tabindex'));

    // Wait one cycle to make sure tabindex is still unchanged afterwards.
    setTimeout(() => {
      assertEquals(null, crInput.getAttribute('tabindex'));
      // Makes sure tabindex is correctly restored after reverting disabled.
      crInput.disabled = false;
      assertEquals('14', crInput.getAttribute('tabindex'));
      assertEquals(14, input.tabIndex);
      done();
    });
  });

  test('pointerdownThenDisabledInSameCycle', function(done) {
    crInput.tabindex = 14;
    // Edge case: pointerdown and disabled are changed in the same cycle.
    crInput.fire('pointerdown');
    crInput.disabled = true;
    assertEquals(null, crInput.getAttribute('tabindex'));

    // Wait one cycle to make sure tabindex is still unchanged afterwards.
    setTimeout(() => {
      assertEquals(null, crInput.getAttribute('tabindex'));
      // Makes sure tabindex is correctly restored after reverting disabled.
      crInput.disabled = false;
      assertEquals('14', crInput.getAttribute('tabindex'));
      assertEquals(14, input.tabIndex);
      done();
    });
  });

  test('placeholderCorrectlyBound', function() {
    assertFalse(input.hasAttribute('placeholder'));

    crInput.placeholder = '';
    assertTrue(input.hasAttribute('placeholder'));

    crInput.placeholder = 'hello';
    assertEquals('hello', input.getAttribute('placeholder'));

    crInput.placeholder = null;
    assertFalse(input.hasAttribute('placeholder'));
  });

  test('labelHiddenWhenEmpty', function() {
    const label = crInput.$.label;
    assertEquals('none', getComputedStyle(crInput.$.label).display);
    crInput.label = 'foobar';
    assertEquals('block', getComputedStyle(crInput.$.label).display);
    assertEquals('foobar', label.textContent.trim());
  });

  test('valueSetCorrectly', function() {
    crInput.value = 'hello';
    assertEquals(crInput.value, input.value);

    // |value| is copied when typing triggers inputEvent.
    input.value = 'hello sir';
    input.dispatchEvent(new InputEvent('input'));
    assertEquals(crInput.value, input.value);
  });

  test('focusState', function() {
    assertFalse(crInput.hasAttribute('focused_'));

    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;

    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);

    let whenTransitionEnd =
        test_util.eventToPromise('transitionend', underline);

    input.focus();
    assertTrue(crInput.hasAttribute('focused_'));
    assertTrue(originalLabelColor != getComputedStyle(label).color);
    return whenTransitionEnd
        .then(() => {
          assertEquals('1', getComputedStyle(underline).opacity);
          assertTrue(0 != underline.offsetWidth);
        })
        .then(() => {
          input.blur();
          whenTransitionEnd =
              test_util.eventToPromise('transitionend', underline);
          // Wait for underline to fade out.
          return whenTransitionEnd;
        })
        .then(() => {
          whenTransitionEnd =
              test_util.eventToPromise('transitionend', underline);
          assertFalse(crInput.hasAttribute('focused_'));
          assertEquals('0', getComputedStyle(underline).opacity);
          // The width transition has a delay larger than the opacity transition
          // duration so that the width can be reset to 0 after the underline is
          // no longer visible.
          return whenTransitionEnd;
        })
        .then(() => {
          assertEquals(0, underline.offsetWidth);
        });
  });

  test('invalidState', function() {
    crInput.errorMessage = 'error';
    const errorLabel = crInput.$.error;
    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;
    const originalLineColor = getComputedStyle(underline).borderBottomColor;

    assertEquals(crInput.errorMessage, errorLabel.textContent);
    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);
    assertEquals('hidden', getComputedStyle(errorLabel).visibility);

    const whenTransitionEnd =
        test_util.eventToPromise('transitionend', underline);

    crInput.invalid = true;
    Polymer.dom.flush();
    assertTrue(crInput.hasAttribute('invalid'));
    assertEquals('visible', getComputedStyle(errorLabel).visibility);
    assertTrue(originalLabelColor != getComputedStyle(label).color);
    assertTrue(
        originalLineColor != getComputedStyle(underline).borderBottomColor);
    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertTrue(0 != underline.offsetWidth);
    });
  });

  test('validation', function() {
    crInput.value = 'FOO';
    crInput.autoValidate = true;
    assertFalse(crInput.hasAttribute('required'));
    assertFalse(crInput.invalid);

    // Note that even with |autoValidate|, crInput.invalid only updates after
    // |value| is changed.
    crInput.setAttribute('required', '');
    assertFalse(crInput.invalid);

    crInput.value = '';
    assertTrue(crInput.invalid);
    crInput.value = 'BAR';
    assertFalse(crInput.invalid);

    const testPattern = '[a-z]+';
    crInput.setAttribute('pattern', testPattern);
    crInput.value = 'FOO';
    assertTrue(crInput.invalid);
    crInput.value = 'foo';
    assertFalse(crInput.invalid);

    // Without |autoValidate|, crInput.invalid should not change even if input
    // value is not valid.
    crInput.autoValidate = false;
    crInput.value = 'ALL CAPS';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
    crInput.value = '';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
  });

  test('numberValidation', function() {
    crInput.type = 'number';
    crInput.value = '50';
    crInput.autoValidate = true;
    assertFalse(crInput.invalid);

    // Note that even with |autoValidate|, crInput.invalid only updates after
    // |value| is changed.
    const testMin = 1;
    const testMax = 100;
    crInput.setAttribute('min', testMin);
    crInput.setAttribute('max', testMax);
    crInput.value = '200';
    assertTrue(crInput.invalid);
    crInput.value = '20';
    assertFalse(crInput.invalid);
    crInput.value = '-2';
    assertTrue(crInput.invalid);
    crInput.value = '40';
    assertFalse(crInput.invalid);

    // Without |autoValidate|, crInput.invalid should not change even if input
    // value is not valid.
    crInput.autoValidate = false;
    crInput.value = '200';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
    crInput.value = '-2';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
  });

  test('ariaLabelsCorrect', function() {
    assertFalse(!!crInput.inputElement.getAttribute('aria-label'));

    /**
     * This function assumes attributes are passed in priority order.
     * @param {!Array<string>} attributes
     */
    function testAriaLabel(attributes) {
      PolymerTest.clearBody();
      crInput = document.createElement('cr-input');
      attributes.forEach(attribute => {
        // Using their name as the value out of convenience.
        crInput.setAttribute(attribute, attribute);
      });
      document.body.appendChild(crInput);
      Polymer.dom.flush();
      // Assuming first attribute takes priority.
      assertEquals(
          attributes[0], crInput.inputElement.getAttribute('aria-label'));
    }

    testAriaLabel(['aria-label', 'label', 'placeholder']);
    testAriaLabel(['label', 'placeholder']);
    testAriaLabel(['placeholder']);
  });

  test('select', function() {
    crInput.value = '0123456789';
    assertFalse(input.matches(':focus'));
    crInput.select();
    assertTrue(input.matches(':focus'));
    assertEquals('0123456789', window.getSelection().toString());

    regenerateNewInput();
    crInput.value = '0123456789';
    assertFalse(input.matches(':focus'));
    crInput.select(2, 6);
    assertTrue(input.matches(':focus'));
    assertEquals('2345', window.getSelection().toString());

    regenerateNewInput();
    crInput.value = '';
    assertFalse(input.matches(':focus'));
    crInput.select();
    assertTrue(input.matches(':focus'));
    assertEquals('', window.getSelection().toString());
  });
});
