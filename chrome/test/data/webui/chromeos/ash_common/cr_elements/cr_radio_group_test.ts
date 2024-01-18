// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';

import {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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
    flush();
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

  function checkPressed(
      keys: string[], initialSelection: string, expectedSelected: string) {
    keys.forEach(key => {
      radioGroup.selected = initialSelection;
      press(key);
      checkSelected(expectedSelected);
    });
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
    checkPressed(['ArrowRight'], '1', '2');
    parent.removeEventListener('keydown', listener);

    // When the key was not handled, the event should propagate.
    const whenBackspace = eventToPromise('keydown', parent);
    checkPressed(['Backspace'], '1', '1');
    await whenBackspace;
  });

  test('key events when initially nothing checked', () => {
    press('Enter');
    checkSelected('1');
    radioGroup.selected = '';
    verifyNoneSelectedOneFocusable('1');
    press(' ');
    checkSelected('1');
    radioGroup.selected = '';
    verifyNoneSelectedOneFocusable('1');
    press('ArrowRight');
    checkSelected('2');
  });

  test('key events when an item is checked', () => {
    checkPressed(['End'], '1', '3');
    checkPressed(['Home'], '3', '1');
    // Check for decrement.
    checkPressed(['Home', 'PageUp', 'ArrowUp', 'ArrowLeft'], '2', '1');
    // No change when reached first selected.
    checkPressed(['Home'], '1', '1');
    // Wraps when decrementing when first selected.
    checkPressed(['PageUp', 'ArrowUp', 'ArrowLeft'], '1', '3');
    // Check for increment.
    checkPressed(['End', 'ArrowRight', 'PageDown', 'ArrowDown'], '2', '3');
    // No change when reached last selected.
    checkPressed(['End'], '3', '3');
    // Wraps when incrementing when last selected.
    checkPressed(['ArrowRight', 'PageDown', 'ArrowDown'], '3', '1');
  });

  test('mouse event', () => {
    assertEquals(undefined, radioGroup.selected);
    radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!.click();
    checkSelected('2');
  });

  test('key events skip over disabled radios', () => {
    verifyNoneSelectedOneFocusable('1');
    radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!.disabled =
        true;
    press('PageDown');
    checkSelected('3');
  });

  test('disabled makes radios not focusable', () => {
    radioGroup.selected = '1';
    checkSelected('1');
    radioGroup.disabled = true;
    checkNoneFocusable();
    radioGroup.disabled = false;
    checkSelected('1');
    const firstRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!;
    firstRadio.disabled = true;
    assertEquals(-1, firstRadio.$.button.tabIndex);
    const secondRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!;
    assertEquals(0, secondRadio.$.button.tabIndex);
    firstRadio.disabled = false;
    checkSelected('1');
    radioGroup.selected = '';
    verifyNoneSelectedOneFocusable('1');
    firstRadio.disabled = true;
    verifyNoneSelectedOneFocusable('2');
  });

  test('when group is disabled, button aria-disabled is updated', () => {
    assertEquals('false', radioGroup.getAttribute('aria-disabled'));
    assertFalse(radioGroup.disabled);
    checkLength(3, '[aria-disabled="false"]');
    radioGroup.disabled = true;
    assertEquals('true', radioGroup.getAttribute('aria-disabled'));
    checkLength(3, '[aria-disabled="true"]');
    radioGroup.disabled = false;
    assertEquals('false', radioGroup.getAttribute('aria-disabled'));
    checkLength(3, '[aria-disabled="false"]');

    // Check that if a button already disabled, it will remain disabled after
    // group is re-enabled.
    const firstRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!;
    firstRadio.disabled = true;
    checkLength(2, '[aria-disabled="false"]');
    checkLength(1, '[aria-disabled="true"][disabled][name="1"]');
    radioGroup.disabled = true;
    checkLength(3, '[aria-disabled="true"]');
    checkLength(1, '[aria-disabled="true"][disabled][name="1"]');
    radioGroup.disabled = false;
    checkLength(2, '[aria-disabled="false"]');
    checkLength(1, '[aria-disabled="true"][disabled][name="1"]');
  });

  test('radios name change updates selection and tabindex', () => {
    radioGroup.selected = '1';
    checkSelected('1');
    const firstRadio =
        radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!;
    firstRadio.name = 'A';
    assertEquals(0, firstRadio.$.button.tabIndex);
    assertFalse(firstRadio.checked);
    verifyNoneSelectedOneFocusable('A');
    radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!.name = '1';
    checkSelected('1');
  });

  test('radios with links', () => {
    const a = radioGroup.querySelector('a');
    assertTrue(!!a);
    assertEquals(-1, a!.tabIndex);
    verifyNoneSelectedOneFocusable('1');
    press('Enter', a!);
    press(' ', a!);
    a!.click();
    verifyNoneSelectedOneFocusable('1');
    radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!.click();
    checkSelected('1');
    press('Enter', a!);
    press(' ', a!);
    a!.click();
    checkSelected('1');
    radioGroup.querySelector<CrRadioButtonElement>('[name="3"]')!.click();
    checkSelected('3');
    assertEquals(0, a!.tabIndex);
  });

  test('radios with input', () => {
    const input = radioGroup.querySelector('input');
    assertTrue(!!input);
    verifyNoneSelectedOneFocusable('1');
    press('Enter', input!);
    press(' ', input!);
    verifyNoneSelectedOneFocusable('1');
    input!.click();
    checkSelected('2');
    radioGroup.querySelector<CrRadioButtonElement>('[name="1"]')!.click();
    press('Enter', input!);
    press(' ', input!);
    checkSelected('1');
    input!.click();
    checkSelected('2');
  });

  test('select the radio that has focus when space or enter pressed', () => {
    verifyNoneSelectedOneFocusable('1');
    press(
        'Enter', radioGroup.querySelector<CrRadioButtonElement>('[name="3"]')!);
    checkSelected('3');
    press(' ', radioGroup.querySelector<CrRadioButtonElement>('[name="2"]')!);
    checkSelected('2');
  });
});
