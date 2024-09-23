// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
// clang-format on

suite('cr-input', function() {
  let crInput: CrInputElement;
  let input: HTMLInputElement;

  setup(function() {
    regenerateNewInput();
  });

  function regenerateNewInput() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crInput = document.createElement('cr-input');
    document.body.appendChild(crInput);
    input = crInput.inputElement;
    flush();
  }

  test('AttributesCorrectlySupported', function() {
    // [externalName, internalName, defaultValue, testValue]
    type AttributeData = [
      keyof CrInputElement, keyof HTMLInputElement, boolean | number | string,
      boolean | number | string
    ];
    const attributesToTest: AttributeData[] = [
      ['autofocus', 'autofocus', false, true],
      ['disabled', 'disabled', false, true],
      ['max', 'max', '', '100'],
      ['min', 'min', '', '1'],
      ['maxlength', 'maxLength', -1, 5],
      ['minlength', 'minLength', -1, 5],
      ['pattern', 'pattern', '', '[a-z]+'],
      ['readonly', 'readOnly', false, true],
      ['required', 'required', false, true],
      ['type', 'type', 'text', 'password'],
      ['inputmode', 'inputMode', '', 'none'],
    ];

    attributesToTest.forEach(
        ([externalName, internalName, defaultValue,
          testValue]: AttributeData) => {
          regenerateNewInput();
          assertEquals(defaultValue, input[internalName]);
          crInput.setAttribute(externalName, testValue.toString());
          assertEquals(testValue, input[internalName]);
        });
  });

  test('UnsupportedInputTabindex', () => {
    assertThrows(() => {
      crInput.inputTabindex = 2;
    });
  });

  test('UnsupportedTypeThrows', function() {
    assertThrows(function() {
      crInput.type = 'checkbox';
    });
  });

  test('inputTabindexCorrectlyBound', () => {
    assertEquals(0, input['tabIndex']);
    crInput.setAttribute('input-tabindex', '-1');
    assertEquals(-1, input.tabIndex);
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
    assertEquals('foobar', label.textContent!.trim());
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

    function waitForTransitions(): Promise<TransitionEvent[]> {
      const events: TransitionEvent[] = [];
      return eventToPromise('transitionend', underline)
          .then(e => {
            events.push(e);
            return eventToPromise('transitionend', underline);
          })
          .then(e => {
            events.push(e);
            return events;
          });
    }

    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);

    let whenTransitionsEnd = waitForTransitions();

    input.focus();
    assertTrue(crInput.hasAttribute('focused_'));
    assertNotEquals(originalLabelColor, getComputedStyle(label).color);
    return whenTransitionsEnd
        .then(events => {
          // Ensure transitions finished in the expected order.
          assertEquals(2, events.length);
          assertEquals('opacity', events[0]!.propertyName);
          assertEquals('width', events[1]!.propertyName);

          assertEquals('1', getComputedStyle(underline).opacity);
          assertNotEquals(0, underline.offsetWidth);

          whenTransitionsEnd = waitForTransitions();
          input.blur();
          return whenTransitionsEnd;
        })
        .then(events => {
          // Ensure transitions finished in the expected order.
          assertEquals(2, events.length);
          assertEquals('opacity', events[0]!.propertyName);
          assertEquals('width', events[1]!.propertyName);

          assertFalse(crInput.hasAttribute('focused_'));
          assertEquals('0', getComputedStyle(underline).opacity);
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

    assertEquals('', errorLabel.textContent);
    assertFalse(errorLabel.hasAttribute('role'));
    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);
    assertEquals('hidden', getComputedStyle(errorLabel).visibility);

    const whenTransitionEnd = eventToPromise('transitionend', underline);

    crInput.invalid = true;
    flush();
    assertTrue(crInput.hasAttribute('invalid'));
    assertEquals('alert', errorLabel.getAttribute('role'));
    assertEquals(crInput.errorMessage, errorLabel.textContent);
    assertEquals('visible', getComputedStyle(errorLabel).visibility);
    assertTrue(originalLabelColor !== getComputedStyle(label).color);
    assertTrue(
        originalLineColor !== getComputedStyle(underline).borderBottomColor);
    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertTrue(0 !== underline.offsetWidth);
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
    crInput.setAttribute('min', testMin.toString());
    crInput.setAttribute('max', testMax.toString());
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

  test('ariaDescriptionsCorrect', function() {
    assertEquals(crInput.inputElement.getAttribute('aria-description'), null);

    const ariaDescription = 'description';
    crInput.ariaDescription = ariaDescription;
    flush();
    assertEquals(
        crInput.inputElement.getAttribute('aria-description'), ariaDescription);

    crInput.ariaDescription = null;
    flush();
    assertEquals(crInput.inputElement.getAttribute('aria-description'), null);
  });

  test('ariaLabelsCorrect', function() {
    assertFalse(!!crInput.inputElement.getAttribute('aria-label'));

    /**
     * This function assumes attributes are passed in priority order.
     */
    function testAriaLabel(attributes: string[]) {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      crInput = document.createElement('cr-input');
      attributes.forEach(attribute => {
        // Using their name as the value out of convenience.
        crInput.setAttribute(attribute, attribute);
      });
      document.body.appendChild(crInput);
      flush();
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
    assertEquals('0123456789', window.getSelection()!.toString());

    regenerateNewInput();
    crInput.value = '0123456789';
    assertFalse(input.matches(':focus'));
    crInput.select(2, 6);
    assertTrue(input.matches(':focus'));
    assertEquals('2345', window.getSelection()!.toString());

    regenerateNewInput();
    crInput.value = '';
    assertFalse(input.matches(':focus'));
    crInput.select();
    assertTrue(input.matches(':focus'));
    assertEquals('', window.getSelection()!.toString());
  });

  test('slots', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-input>
        <div slot="inline-prefix" id="inline-prefix">One</div>
        <div slot="suffix" id="suffix">Two</div>
        <div slot="inline-suffix" id="inline-suffix">Three</div>
      </cr-input>
    `;
    flush();
    crInput = document.querySelector('cr-input')!;
    assertTrue(isChildVisible(crInput, '#inline-prefix', true));
    assertTrue(isChildVisible(crInput, '#suffix', true));
    assertTrue(isChildVisible(crInput, '#inline-suffix', true));
  });
});
