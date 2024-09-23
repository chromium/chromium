// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';

import type {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-radio-group', () => {
  let radioGroup: CrRadioGroupElement;

  setup(() => {
    document.body.innerHTML = getTrustedHTML`
       <div id="parent">
          <cr-radio-group>
            <cr-radio-button name="1"></cr-radio-button>
            <cr-radio-button name="2"><input></input></cr-radio-button>
            <cr-radio-button name="3"><a></a></cr-radio-button>
          </cr-radio-group>
        </div>`;
    radioGroup = document.body.querySelector('cr-radio-group')!;
    return microtasksFinished();
  });

  function checkLength(length: number, selector: string) {
    assertEquals(length, radioGroup.querySelectorAll(selector).length);
  }

  function verifyNoneSelectedOneFocusable(name: string) {
    const uncheckedRows =
        Array.from(radioGroup.querySelectorAll<CrRadioButtonElement>(
            `cr-radio-button:not([checked])`));
    assertEquals(3, uncheckedRows.length);

    const focusableRow = uncheckedRows.filter(
        radioButton =>
            radioButton.name === name && radioButton.$.button.tabIndex === 0);
    assertEquals(1, focusableRow.length);

    const unfocusableRows = uncheckedRows.filter(
        radioButton => radioButton.$.button.tabIndex === -1);
    assertEquals(2, unfocusableRows.length);
  }

  function checkNoneFocusable() {
    const allRows = Array.from(radioGroup.querySelectorAll(`cr-radio-button`));
    assertEquals(3, allRows.length);

    const unfocusableRows =
        allRows.filter(radioButton => radioButton.$.button.tabIndex === -1);
    assertEquals(3, unfocusableRows.length);
  }

  function press(key: string, target?: Element) {
    pressAndReleaseKeyOn(
        target || radioGroup.querySelector('[name="1"]')!, -1, [], key);
  }

  async function checkPressed(
      keys: string[], initialSelection: string, expectedSelected: string) {
    for (const key of keys) {
      radioGroup.selected = initialSelection;
      await microtasksFinished();
      press(key);
      await microtasksFinished();
      checkSelected(expectedSelected);
    }
  }

  function checkSelected(name: string) {
    assertEquals(name, `${radioGroup.selected}`);

    const selectedRows =
        Array.from(radioGroup.querySelectorAll<CrRadioButtonElement>(
            `cr-radio-button[name="${name}"][checked]`));
    const focusableRows =
        selectedRows.filter(radioButton => radioButton.$.button.tabIndex === 0);
    assertEquals(1, focusableRows.length);

    const unselectedRows =
        Array.from(radioGroup.querySelectorAll<CrRadioButtonElement>(
            `cr-radio-button:not([name="${name}"]):not([checked])`));
    const filteredUnselected = unselectedRows.filter(
        radioButton => radioButton.$.button.tabIndex === -1);
    assertEquals(2, filteredUnselected.length);
  }

  test('selected-changed bubbles', () => {
    const whenFired = eventToPromise('selected-changed', radioGroup);
    radioGroup.selected = '1';
    return whenFired;
  });

  test('key events don\'t propagate to parents', async () => {
    const parent = document.body.querySelector<HTMLElement>('#parent');
    assertTrue(!!parent);

    // When the key was handled, the event should not propagate. Not using
    // eventToPromise on purpose, as Mocha fails to capture the error if it
    // happens in a Promise that is not awaited.
    const listener = () => {
      assertNotReached('Event should not have bubbled to parent.');
    };
    parent.addEventListener('keydown', listener, {once: true});
    await checkPressed(['ArrowRight'], '1', '2');
    parent.removeEventListener('keydown', listener);

    // When the key was not handled, the event should propagate.
    const whenBackspace = eventToPromise('keydown', parent);
    await checkPressed(['Backspace'], '1', '1');
    await whenBackspace;
  });

  test('key events when initially nothing checked', async () => {
    press('Enter');
    await microtasksFinished();
    checkSelected('1');

    radioGroup.selected = '';
    await microtasksFinished();
    verifyNoneSelectedOneFocusable('1');

    press(' ');
    await microtasksFinished();
    checkSelected('1');

    radioGroup.selected = '';
    await microtasksFinished();
    verifyNoneSelectedOneFocusable('1');

    press('ArrowRight');
    await microtasksFinished();
    checkSelected('2');
  });

  test('key events when an item is checked', async () => {
    await checkPressed(['End'], '1', '3');
    await checkPressed(['Home'], '3', '1');
    // Check for decrement.
    await checkPressed(['Home', 'PageUp', 'ArrowUp', 'ArrowLeft'], '2', '1');
    // No change when reached first selected.
    await checkPressed(['Home'], '1', '1');
    // Wraps when decrementing when first selected.
    await checkPressed(['PageUp', 'ArrowUp', 'ArrowLeft'], '1', '3');
    // Check for increment.
    await checkPressed(
        ['End', 'ArrowRight', 'PageDown', 'ArrowDown'], '2', '3');
    // No change when reached last selected.
    await checkPressed(['End'], '3', '3');
    // Wraps when incrementing when last selected.
    await checkPressed(['ArrowRight', 'PageDown', 'ArrowDown'], '3', '1');
  });

  test('mouse event', async () => {
    assertEquals(undefined, radioGroup.selected);
    radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!.click();
    await microtasksFinished();
    checkSelected('2');
  });

  test('key events skip over disabled radios', async () => {
    verifyNoneSelectedOneFocusable('1');
    radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!.disabled =
        true;
    await microtasksFinished();
    press('PageDown');
    await microtasksFinished();
    checkSelected('3');
  });

  test('disabled makes radios not focusable', async () => {
    radioGroup.selected = '1';
    await microtasksFinished();
    checkSelected('1');

    radioGroup.disabled = true;
    await microtasksFinished();
    checkNoneFocusable();

    radioGroup.disabled = false;
    await microtasksFinished();
    checkSelected('1');

    const firstRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!;
    firstRadio.disabled = true;
    await microtasksFinished();
    assertEquals(-1, firstRadio.$.button.tabIndex);

    const secondRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!;
    assertEquals(0, secondRadio.$.button.tabIndex);
    firstRadio.disabled = false;
    await microtasksFinished();
    checkSelected('1');

    radioGroup.selected = '';
    await microtasksFinished();
    verifyNoneSelectedOneFocusable('1');

    firstRadio.disabled = true;
    await microtasksFinished();
    verifyNoneSelectedOneFocusable('2');
  });

  test('when group is disabled, button aria-disabled is updated', async () => {
    assertEquals('false', radioGroup.getAttribute('aria-disabled'));
    assertFalse(radioGroup.disabled);
    checkLength(3, '[aria-disabled="false"]');
    radioGroup.disabled = true;
    await radioGroup.updateComplete;
    assertEquals('true', radioGroup.getAttribute('aria-disabled'));
    await microtasksFinished();
    checkLength(3, '[aria-disabled="true"]');

    radioGroup.disabled = false;
    await radioGroup.updateComplete;
    assertEquals('false', radioGroup.getAttribute('aria-disabled'));
    await microtasksFinished();
    checkLength(3, '[aria-disabled="false"]');

    // Check that if a button already disabled, it will remain disabled after
    // group is re-enabled.
    const firstRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!;
    firstRadio.disabled = true;
    await microtasksFinished();
    checkLength(2, '[aria-disabled="false"]');
    checkLength(1, '[aria-disabled="true"][disabled][name="1"]');

    radioGroup.disabled = true;
    await microtasksFinished();
    checkLength(3, '[aria-disabled="true"]');
    checkLength(1, '[aria-disabled="true"][disabled][name="1"]');

    radioGroup.disabled = false;
    await microtasksFinished();
    checkLength(2, '[aria-disabled="false"]');
    checkLength(1, '[aria-disabled="true"][disabled][name="1"]');
  });

  test('radios name change updates selection and tabindex', async () => {
    radioGroup.selected = '1';
    await microtasksFinished();
    checkSelected('1');
    const firstRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!;
    firstRadio.name = 'A';
    await microtasksFinished();
    assertEquals(0, firstRadio.$.button.tabIndex);
    assertFalse(firstRadio.checked);
    verifyNoneSelectedOneFocusable('A');
    radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!.name = '1';
    await microtasksFinished();
    checkSelected('1');
  });

  test('radios with links', async () => {
    const a = radioGroup.querySelector('a');
    assertTrue(!!a);
    assertEquals(-1, a!.tabIndex);
    verifyNoneSelectedOneFocusable('1');
    press('Enter', a!);
    await radioGroup.updateComplete;
    press(' ', a!);
    await radioGroup.updateComplete;
    a!.click();
    await radioGroup.updateComplete;
    verifyNoneSelectedOneFocusable('1');

    radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!.click();
    await microtasksFinished();
    checkSelected('1');
    press('Enter', a!);
    await radioGroup.updateComplete;
    press(' ', a!);
    await radioGroup.updateComplete;
    a!.click();
    await microtasksFinished();
    checkSelected('1');

    radioGroup.querySelector<CrRadioButtonElement>('[name="3"]')!.click();
    await microtasksFinished();
    checkSelected('3');
    assertEquals(0, a!.tabIndex);
  });

  test('radios with input', async () => {
    const input = radioGroup.querySelector('input');
    assertTrue(!!input);
    verifyNoneSelectedOneFocusable('1');
    press('Enter', input!);
    press(' ', input!);
    await radioGroup.updateComplete;
    verifyNoneSelectedOneFocusable('1');

    input!.click();
    await microtasksFinished();
    checkSelected('2');

    radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!.click();
    await microtasksFinished();
    press('Enter', input!);
    press(' ', input!);
    await microtasksFinished();
    checkSelected('1');

    input!.click();
    await microtasksFinished();
    checkSelected('2');
  });

  test(
      'select the radio that has focus when space or enter pressed',
      async () => {
        verifyNoneSelectedOneFocusable('1');
        press(
            'Enter',
            radioGroup.querySelector<CrRadioButtonElement>('[name="3"]')!);
        await microtasksFinished();
        checkSelected('3');
        press(
            ' ', radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!);
        await microtasksFinished();
        checkSelected('2');
      });

  // Test that when a 2 way binding to a Polymer parent is updated, the
  // radio buttons UI state has already been updated.
  test('TwoWayBindingWithPolymerParent', async () => {
    class TestElement extends PolymerElement {
      static get is() {
        return 'test-element';
      }

      static get template() {
        return html`
          <cr-radio-group
              selected="{{parentSelected}}"
              on-selected-changed="onSelectedChanged">
            <cr-radio-button name="one">Option 1</cr-radio-button>
            <cr-radio-button name="two">Option 2</cr-radio-button>
          </cr-radio-group>`;
      }

      static get properties() {
        return {
          parentSelected: String,
        };
      }

      parentSelected: string = 'one';
      changes: string[] = [];

      onSelectedChanged(e: CustomEvent<{value: string}>) {
        const buttons = this.shadowRoot!.querySelectorAll('cr-radio-button');
        assertEquals(2, buttons.length);
        // Verify that the buttons' checked state is already up to date.
        const isOne = e.detail.value === 'one';
        assertEquals(isOne, buttons[0]!.checked);
        assertEquals(!isOne, buttons[1]!.checked);
        this.changes.push(e.detail.value);
      }
    }

    customElements.define(TestElement.is, TestElement);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);
    await microtasksFinished();

    const radioGroup = element.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('one', radioGroup.selected);
    assertEquals('one', element.parentSelected);

    const radioButtons =
        element.shadowRoot!.querySelectorAll('cr-radio-button');
    assertEquals(2, radioButtons.length);
    radioButtons[1]!.click();
    await microtasksFinished();
    assertEquals('two', radioGroup.selected);
    assertEquals('two', element.parentSelected);

    assertDeepEquals(['one', 'two'], element.changes);
  });
});
