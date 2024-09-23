// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
// clang-format on

suite('cr-input', function() {
  let crInput: CrInputElement;
  let input: HTMLInputElement;

  setup(function() {
    return regenerateNewInput();
  });

  async function regenerateNewInput(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crInput = document.createElement('cr-input');
    document.body.appendChild(crInput);
    input = crInput.inputElement;
    await crInput.updateComplete;
  }

  test('AttributesCorrectlySupported', async () => {
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

    for (const [externalName, internalName, defaultValue, testValue] of
             attributesToTest) {
      await regenerateNewInput();
      assertEquals(defaultValue, input[internalName]);
      crInput.setAttribute(externalName, testValue.toString());
      await crInput.updateComplete;
      assertEquals(testValue, input[internalName]);
    }
  });

  test('UnsupportedInputTabindex', async () => {
    let errorThrown = false;
    try {
      crInput.inputTabindex = 2;
      await crInput.updateComplete;
    } catch (e) {
      errorThrown = true;
    }
    assertTrue(errorThrown);
  });

  test('UnsupportedTypeThrows', async () => {
    let errorThrown = false;
    try {
      crInput.type = 'checkbox';
      await crInput.updateComplete;
    } catch (e) {
      errorThrown = true;
    }
    assertTrue(errorThrown);
  });

  test('inputTabindexCorrectlyBound', async () => {
    assertEquals(0, input['tabIndex']);
    crInput.setAttribute('input-tabindex', '-1');
    await crInput.updateComplete;
    assertEquals(-1, input.tabIndex);
  });

  test('placeholderCorrectlyBound', async () => {
    assertFalse(input.hasAttribute('placeholder'));

    crInput.placeholder = '';
    await crInput.updateComplete;
    assertTrue(input.hasAttribute('placeholder'));

    crInput.placeholder = 'hello';
    await crInput.updateComplete;
    assertEquals('hello', input.getAttribute('placeholder'));

    crInput.placeholder = null;
    await crInput.updateComplete;
    assertFalse(input.hasAttribute('placeholder'));
  });

  test('labelHiddenWhenEmpty', async () => {
    const label = crInput.$.label;
    assertEquals('none', getComputedStyle(crInput.$.label).display);
    crInput.label = 'foobar';
    await crInput.updateComplete;
    assertEquals('block', getComputedStyle(crInput.$.label).display);
    assertEquals('foobar', label.textContent!.trim());
  });

  test('valueSetCorrectly', async () => {
    crInput.value = 'hello';
    await crInput.updateComplete;
    assertEquals(crInput.value, input.value);

    // |value| is copied when typing triggers inputEvent.
    input.value = 'hello sir';
    input.dispatchEvent(new InputEvent('input'));
    await crInput.updateComplete;
    assertEquals(crInput.value, input.value);
  });

  test('focusState', async () => {
    assertFalse(crInput.hasAttribute('focused_'));

    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;

    async function waitForTransitions(): Promise<TransitionEvent[]> {
      const events: TransitionEvent[] = [];
      let e = await eventToPromise('transitionend', underline);
      events.push(e);
      e = await eventToPromise('transitionend', underline);
      events.push(e);
      return events;
    }

    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);

    let whenTransitionsEnd = waitForTransitions();

    input.focus();
    await crInput.updateComplete;
    assertTrue(crInput.hasAttribute('focused_'));
    assertEquals(originalLabelColor, getComputedStyle(label).color);
    let events = await whenTransitionsEnd;
    // Ensure transitions finished in the expected order.
    assertEquals(2, events.length);
    assertEquals('opacity', events[0]!.propertyName);
    assertEquals('width', events[1]!.propertyName);

    assertEquals('1', getComputedStyle(underline).opacity);
    assertNotEquals(0, underline.offsetWidth);

    whenTransitionsEnd = waitForTransitions();
    input.blur();
    events = await whenTransitionsEnd;
    // Ensure transitions finished in the expected order.
    assertEquals(2, events.length);
    assertEquals('opacity', events[0]!.propertyName);
    assertEquals('width', events[1]!.propertyName);

    assertFalse(crInput.hasAttribute('focused_'));
    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);
  });

  test('invalidState', async () => {
    crInput.errorMessage = 'error';
    await crInput.updateComplete;
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
    await crInput.updateComplete;
    assertTrue(crInput.hasAttribute('invalid'));
    assertEquals('alert', errorLabel.getAttribute('role'));
    assertEquals(crInput.errorMessage, errorLabel.textContent);
    assertEquals('visible', getComputedStyle(errorLabel).visibility);
    assertTrue(originalLabelColor !== getComputedStyle(label).color);
    assertTrue(
        originalLineColor !== getComputedStyle(underline).borderBottomColor);

    await whenTransitionEnd;
    assertEquals('1', getComputedStyle(underline).opacity);
    assertTrue(0 !== underline.offsetWidth);
  });

  test('validation', async () => {
    crInput.value = 'FOO';
    crInput.autoValidate = true;
    await crInput.updateComplete;
    assertFalse(crInput.hasAttribute('required'));
    assertFalse(crInput.invalid);

    // Note that even with |autoValidate|, crInput.invalid only updates after
    // |value| is changed.
    crInput.setAttribute('required', '');
    await crInput.updateComplete;
    assertFalse(crInput.invalid);

    crInput.value = '';
    await crInput.updateComplete;
    assertTrue(crInput.invalid);
    crInput.value = 'BAR';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);

    const testPattern = '[a-z]+';
    crInput.setAttribute('pattern', testPattern);
    crInput.value = 'FOO';
    await crInput.updateComplete;
    assertTrue(crInput.invalid);
    crInput.value = 'foo';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);

    // Without |autoValidate|, crInput.invalid should not change even if input
    // value is not valid.
    crInput.autoValidate = false;
    crInput.value = 'ALL CAPS';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
    crInput.value = '';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
  });

  test('numberValidation', async () => {
    crInput.type = 'number';
    crInput.value = '50';
    crInput.autoValidate = true;
    await crInput.updateComplete;
    assertFalse(crInput.invalid);

    // Note that even with |autoValidate|, crInput.invalid only updates after
    // |value| is changed.
    const testMin = 1;
    const testMax = 100;
    crInput.setAttribute('min', testMin.toString());
    crInput.setAttribute('max', testMax.toString());
    crInput.value = '200';
    await crInput.updateComplete;
    assertTrue(crInput.invalid);
    crInput.value = '20';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);
    crInput.value = '-2';
    await crInput.updateComplete;
    assertTrue(crInput.invalid);
    crInput.value = '40';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);

    // Without |autoValidate|, crInput.invalid should not change even if input
    // value is not valid.
    crInput.autoValidate = false;
    crInput.value = '200';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
    crInput.value = '-2';
    await crInput.updateComplete;
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
  });

  test('ariaDescriptionsCorrect', async () => {
    assertEquals(crInput.inputElement.getAttribute('aria-description'), null);

    const ariaDescription = 'description';
    crInput.ariaDescription = ariaDescription;
    await crInput.updateComplete;
    assertEquals(
        crInput.inputElement.getAttribute('aria-description'), ariaDescription);

    crInput.ariaDescription = null;
    await crInput.updateComplete;
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
      // Assuming first attribute takes priority.
      assertEquals(
          attributes[0], crInput.inputElement.getAttribute('aria-label'));
    }

    testAriaLabel(['aria-label', 'label', 'placeholder']);
    testAriaLabel(['label', 'placeholder']);
    testAriaLabel(['placeholder']);
  });

  test('select', async () => {
    crInput.value = '0123456789';
    await crInput.updateComplete;
    assertFalse(input.matches(':focus'));
    crInput.select();
    assertTrue(input.matches(':focus'));
    assertEquals('0123456789', window.getSelection()!.toString());

    await regenerateNewInput();
    crInput.value = '0123456789';
    await crInput.updateComplete;
    assertFalse(input.matches(':focus'));
    crInput.select(2, 6);
    assertTrue(input.matches(':focus'));
    assertEquals('2345', window.getSelection()!.toString());

    await regenerateNewInput();
    crInput.value = '';
    await crInput.updateComplete;
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
    crInput = document.querySelector('cr-input')!;
    assertTrue(isChildVisible(crInput, '#inline-prefix', true));
    assertTrue(isChildVisible(crInput, '#suffix', true));
    assertTrue(isChildVisible(crInput, '#inline-suffix', true));
  });

  // Test that 2-way bindings with Polymer parent elements are updated
  // when validate() is called and before the change event fires
  test('TwoWayBindingWithPolymerParent', async () => {
    class TestElement extends PolymerElement {
      static get is() {
        return 'test-element';
      }

      static get template() {
        return html`
          <cr-input value="{{parentValue}}" invalid="{{parentInvalid}}"
              required on-invalid-changed="onInvalidChanged"
              on-value-changed="onValueChanged"
              on-change="onChange">
          </cr-input>`;
      }

      static get properties() {
        return {
          parentValue: String,
          parentInvalid: Boolean,
        };
      }

      parentValue: string = 'hello';
      parentInvalid: boolean = false;
      private events_: string[] = [];
      private expectedValue_: string = 'hello';

      onInvalidChanged(e: CustomEvent<{value: boolean}>) {
        assertEquals(this.expectedValue_ === '', e.detail.value);
        this.events_.push(e.type);
      }

      onChange(e: CustomEvent<{sourceEvent: InputEvent}>) {
        // Ensure the 2 way binding has already updated.
        assertEquals(this.expectedValue_, this.parentValue);
        assertEquals(this.expectedValue_, e.detail.sourceEvent.detail);
        this.events_.push(e.type);
      }

      onValueChanged(e: CustomEvent<{value: string}>) {
        assertEquals(this.expectedValue_, e.detail.value);
        this.events_.push(e.type);
      }

      validateEvents(expectedEvents: string[]) {
        assertDeepEquals(expectedEvents, this.events_);
        this.events_ = [];
      }

      setExpectedValue(value: string) {
        this.expectedValue_ = value;
      }
    }

    customElements.define(TestElement.is, TestElement);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    const input = element.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    await input.updateComplete;
    // Initialization events
    element.validateEvents(['invalid-changed', 'value-changed']);

    function simulateUserInput(inputValue: string): Promise<void> {
      element.setExpectedValue(inputValue);
      assertTrue(!!input);
      input.inputElement.value = inputValue;
      input.inputElement.dispatchEvent(
          new CustomEvent('input', {composed: true, bubbles: true}));
      input.inputElement.dispatchEvent(
          new CustomEvent('change', {detail: inputValue}));
      return eventToPromise('change', input);
    }

    // Clear the input. This makes it invalid since |required| is set.
    await simulateUserInput('');
    let valid = input.validate();
    assertFalse(valid);
    // 2 way binding should have updated since validate() was called.
    assertTrue(element.parentInvalid);
    element.validateEvents(['value-changed', 'change', 'invalid-changed']);

    // Reset to a valid input value.
    await simulateUserInput('test');
    valid = input.validate();
    assertTrue(valid);
    // 2 way binding should have updated since validate() was called.
    assertFalse(element.parentInvalid);
    element.validateEvents(['value-changed', 'change', 'invalid-changed']);
  });
});
