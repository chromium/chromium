// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/combobox/customize_chrome_combobox.js';

import {CustomizeChromeCombobox, OptionElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/combobox/customize_chrome_combobox.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('ComboboxTest', () => {
  let combobox: CustomizeChromeCombobox;

  function addGroup(): HTMLElement {
    const group = document.createElement('div');
    group.setAttribute('role', 'group');
    const label = document.createElement('label');
    label.innerText = 'Group';
    group.appendChild(label);
    combobox.appendChild(group);
    return label;
  }

  function addOption(
      parent: HTMLElement = combobox, value: string = 'value'): OptionElement {
    const option = document.createElement('div') as OptionElement;
    option.setAttribute('role', 'option');
    option.innerText = 'Option';
    option.value = value;
    parent.appendChild(option);
    return option;
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    combobox = document.createElement('customize-chrome-combobox');
    document.body.appendChild(combobox);
  });

  test('ShowsAndHides', () => {
    addOption();
    assertFalse(isVisible(combobox.$.dropdown));
    combobox.$.input.click();
    assertTrue(isVisible(combobox.$.dropdown));

    combobox.$.input.dispatchEvent(new FocusEvent('focusout'));
    assertFalse(isVisible(combobox.$.dropdown));
  });

  test('OpensAndClosesDropdownOnKeydown', async () => {
    const option1 = addOption();
    const option2 = addOption();
    await flushTasks();
    assertFalse(isVisible(combobox.$.dropdown));

    function assertDropdownOpensAndHighlightsFirst(
        key: string, expectedHighlight: HTMLElement) {
      combobox.dispatchEvent(new KeyboardEvent('keydown', {key}));
      assertTrue(isVisible(combobox.$.dropdown));
      assertEquals(expectedHighlight, combobox.querySelector('[highlighted]'));
      // Close the dropdown.
      combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    }

    assertDropdownOpensAndHighlightsFirst('ArrowDown', option1);
    assertDropdownOpensAndHighlightsFirst('ArrowUp', option2);
    assertDropdownOpensAndHighlightsFirst('Home', option1);
    assertDropdownOpensAndHighlightsFirst('End', option2);
    assertDropdownOpensAndHighlightsFirst('Enter', option1);
    assertDropdownOpensAndHighlightsFirst('Space', option1);
  });

  test('HighlightsItemsOnKeydownWhenOpen', async () => {
    const groupA = addGroup();
    const optionA1 = addOption(groupA);
    const optionA2 = addOption(groupA);
    const groupB = addGroup();
    const optionB1 = addOption(groupB);
    await flushTasks();

    // ArrowDown should loop through list.
    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(groupA, combobox.querySelector('[highlighted]'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(optionA1, combobox.querySelector('[highlighted]'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(optionA2, combobox.querySelector('[highlighted]'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(groupB, combobox.querySelector('[highlighted]'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(optionB1, combobox.querySelector('[highlighted]'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(groupA, combobox.querySelector('[highlighted]'));

    // ArrowUp goes reverse order.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    assertEquals(optionB1, combobox.querySelector('[highlighted]'));

    // Home and End keys work.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Home'}));
    assertEquals(groupA, combobox.querySelector('[highlighted]'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'End'}));
    assertEquals(optionB1, combobox.querySelector('[highlighted]'));

    // Closes when hitting Escape and resets highlight.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    assertFalse(isVisible(combobox.$.dropdown));
    assertEquals(null, combobox.querySelector('[highlighted]'));
  });

  test('HighlightsOnPointerover', async () => {
    const group = addGroup();
    const option = addOption(group);
    await flushTasks();

    // Open the dropdown.
    combobox.$.input.click();

    group.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(group, combobox.querySelector('[highlighted]'));
    option.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(option, combobox.querySelector('[highlighted]'));
  });

  test('HighlightsOnPointermoveAfterKeyEvent', async () => {
    const option1 = addOption();
    const option2 = addOption();
    await flushTasks();

    // Open the dropdown.
    combobox.$.input.click();

    // Mouse moves to first option.
    option1.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(option1, combobox.querySelector('[highlighted]'));

    // Keydown down to highlight second option. Mouse still over first option.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(option2, combobox.querySelector('[highlighted]'));

    // Pointerover event over first option should not highlight first option,
    // since it follows a key event.
    option1.dispatchEvent(new PointerEvent('pointerover'));
    assertEquals(option2, combobox.querySelector('[highlighted]'));

    // Mock moving mouse within the first option again.
    option1.dispatchEvent(
        new PointerEvent('pointermove', {bubbles: true, composed: true}));
    assertEquals(option1, combobox.querySelector('[highlighted]'));
  });

  test('SelectsItem', async () => {
    const groupA = addGroup();
    const optionA1 = addOption(groupA);
    optionA1.innerText = 'I am option 1';
    const optionA2 = addOption(groupA);
    optionA2.innerText = 'I am option 2';
    await flushTasks();

    // Open dropdown, click on first option to select it.
    combobox.$.input.click();
    optionA1.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    assertTrue(optionA1.hasAttribute('selected'));
    assertFalse(isVisible(combobox.$.dropdown));
    assertTrue(combobox.$.input.textContent!.includes('I am option 1'));

    // Open the dropdown back and arrow key to next option and select it.
    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertFalse(optionA2.hasAttribute('selected'));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertTrue(optionA2.hasAttribute('selected'));
    assertFalse(optionA1.hasAttribute('selected'));
    assertTrue(combobox.$.input.textContent!.includes('I am option 2'));
    assertFalse(isVisible(combobox.$.dropdown));

    // Pressing Enter or clicking on an unselectable item should not select it.
    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Home'}));
    const groupAClickEvent = eventToPromise('click', groupA);
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await groupAClickEvent;
    assertFalse(groupA.hasAttribute('selected'));
    groupA.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    assertFalse(groupA.hasAttribute('selected'));
    assertTrue(optionA2.hasAttribute('selected'));
    assertTrue(isVisible(combobox.$.dropdown));
  });

  test('NotifiesValueChange', async () => {
    const option1 = addOption();
    option1.value = 'option-1-value';
    const option2 = addOption();
    option2.value = 'option-2-value';

    let valueChangeEvent = eventToPromise('value-changed', combobox);
    combobox.$.input.click();
    option1.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    await valueChangeEvent;
    assertEquals('option-1-value', combobox.value);
    assertTrue(option1.hasAttribute('selected'));

    valueChangeEvent = eventToPromise('value-changed', combobox);
    combobox.$.input.click();
    option2.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    assertEquals('option-2-value', combobox.value);
    assertTrue(option2.hasAttribute('selected'));
  });

  test('UpdatesWithBoundValue', async () => {
    const option1 = addOption();
    option1.value = 'option-1-value';
    const option2 = addOption();
    option2.value = 'option-2-value';

    combobox.value = 'option-1-value';
    assertTrue(option1.hasAttribute('selected'));
    assertFalse(option2.hasAttribute('selected'));

    combobox.value = 'option-2-value';
    assertFalse(option1.hasAttribute('selected'));
    assertTrue(option2.hasAttribute('selected'));
  });

  test('UpdatesWithBoundValueOnDomChange', async () => {
    const groupA = addGroup();
    addOption(groupA, 'valueA1');
    const groupB = addGroup();

    // Set the bound value to an option that does not exist yet.
    combobox.value = 'valueB1';
    await flushTasks();
    assertEquals(0, combobox.querySelectorAll('[selected]').length);

    // Add an option with the selected value.
    const optionB1 = addOption(groupB, 'valueB1');
    await flushTasks();
    assertEquals(1, combobox.querySelectorAll('[selected]').length);
    assertTrue(optionB1.hasAttribute('selected'));

    // Removing the option should keep the bound value.
    optionB1.remove();
    await flushTasks();
    assertEquals('valueB1', combobox.value);
    assertEquals(0, combobox.querySelectorAll('[selected]').length);

    // Adding a new option with the same value should select it.
    const newOptionB1 = addOption(groupB, 'valueB1');
    await flushTasks();
    assertEquals(1, combobox.querySelectorAll('[selected]').length);
    assertTrue(newOptionB1.hasAttribute('selected'));
  });

  test('SetsUniqueIdsAndAriaActiveDescendant', async () => {
    const option1 = addOption();
    option1.value = 'option-1-value';
    const option2 = addOption();
    option2.value = 'option-2-value';
    await flushTasks();

    assertTrue(option1.id.includes('comboboxItem'));
    assertTrue(option2.id.includes('comboboxItem'));
    assertNotEquals(option1.id, option2.id);

    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        option1.id, combobox.$.input.getAttribute('aria-activedescendant'));

    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        option2.id, combobox.$.input.getAttribute('aria-activedescendant'));
  });
});
