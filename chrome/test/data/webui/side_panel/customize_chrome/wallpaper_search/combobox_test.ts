// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/combobox/customize_chrome_combobox.js';

import {CustomizeChromeCombobox} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/combobox/customize_chrome_combobox.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('ComboboxTest', () => {
  let combobox: CustomizeChromeCombobox;

  function getGroup(groupIndex: number): HTMLElement {
    return combobox.shadowRoot!.querySelectorAll('[role=group]')[groupIndex] as
        HTMLElement;
  }

  function getOptionFromGroup(
      groupIndex: number, optionIndex: number): HTMLElement {
    return getGroup(groupIndex)
               .querySelectorAll('[role=option]')[optionIndex] as HTMLElement;
  }

  function getDefaultOption(): HTMLElement {
    return combobox.shadowRoot!.querySelector('#defaultOption')!;
  }

  function getOption(optionIndex: number): HTMLElement {
    return combobox.shadowRoot!.querySelectorAll(
               '[role=option]:not(#defaultOption)')[optionIndex] as HTMLElement;
  }

  function toggleGroupExpand(groupIndex: number) {
    getGroup(groupIndex)
        .querySelector('label')!.dispatchEvent(new Event('click'));
  }

  function getHighlightedElement() {
    return combobox.shadowRoot!.querySelector('[highlighted]');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    combobox = document.createElement('customize-chrome-combobox');
    combobox.label = 'Label';
    combobox.defaultOptionLabel = 'Select a option';
    combobox.items = [
      {label: 'Option 1'},
      {label: 'Option 2'},
    ];
    document.body.appendChild(combobox);
    return flushTasks();
  });

  test('ShowsAndHides', () => {
    assertFalse(isVisible(combobox.$.dropdown));
    combobox.$.input.click();
    assertTrue(isVisible(combobox.$.dropdown));

    combobox.$.input.dispatchEvent(new FocusEvent('focusout'));
    assertFalse(isVisible(combobox.$.dropdown));
  });

  test('OpensAndClosesDropdownOnKeydown', async () => {
    function assertDropdownOpensAndHighlightsFirst(
        key: string, expectedHighlight: HTMLElement) {
      combobox.dispatchEvent(new KeyboardEvent('keydown', {key}));
      assertTrue(isVisible(combobox.$.dropdown));
      assertEquals(
          expectedHighlight,
          combobox.shadowRoot!.querySelector('[highlighted]'));
      // Close the dropdown.
      combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    }

    assertDropdownOpensAndHighlightsFirst('ArrowDown', getDefaultOption());
    assertDropdownOpensAndHighlightsFirst('ArrowUp', getOption(1));
    assertDropdownOpensAndHighlightsFirst('Home', getDefaultOption());
    assertDropdownOpensAndHighlightsFirst('End', getOption(1));
    assertDropdownOpensAndHighlightsFirst('Enter', getDefaultOption());
    assertDropdownOpensAndHighlightsFirst('Space', getDefaultOption());
  });

  test('HighlightsItemsOnKeydownWhenOpen', async () => {
    combobox.items = [
      {
        label: 'Group A',
        items: [{label: 'Option A1'}, {label: 'OptionA2'}],
      },
      {
        label: 'Group B',
        items: [{label: 'Option B1'}],
      },
    ];
    await flushTasks();

    toggleGroupExpand(0);
    toggleGroupExpand(1);
    await flushTasks();

    const groupA = getGroup(0);
    const optionA1 = getOptionFromGroup(0, 0);
    const optionA2 = getOptionFromGroup(0, 1);
    const groupB = getGroup(1);
    const optionB1 = getOptionFromGroup(1, 0);

    // ArrowDown should loop through list.
    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(getDefaultOption(), getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(groupA.querySelector('label'), getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(optionA1, getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(optionA2, getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(groupB.querySelector('label'), getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(optionB1, getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(getDefaultOption(), getHighlightedElement());

    // ArrowUp goes reverse order.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    assertEquals(optionB1, getHighlightedElement());

    // Home and End keys work.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Home'}));
    assertEquals(getDefaultOption(), getHighlightedElement());
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'End'}));
    assertEquals(optionB1, getHighlightedElement());

    // Closes when hitting Escape and resets highlight.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    assertFalse(isVisible(combobox.$.dropdown));
    assertEquals(null, getHighlightedElement());
  });

  test('HighlightsOnPointerover', async () => {
    combobox.items = [
      {
        label: 'Group A',
        items: [{label: 'Option A1'}],
      },
    ];
    await flushTasks();
    toggleGroupExpand(0);
    await flushTasks();

    const group = getGroup(0);
    const option = getOptionFromGroup(0, 0);

    // Open the dropdown.
    combobox.$.input.click();

    group.querySelector('label')!.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(group.querySelector('label'), getHighlightedElement());
    option.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(option, getHighlightedElement());
  });

  test('HighlightsOnPointermoveAfterKeyEvent', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);

    // Open the dropdown.
    combobox.$.input.click();

    // Mouse moves to first option.
    option1.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(option1, getHighlightedElement());

    // Keydown down to highlight second option. Mouse still over first option.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(option2, getHighlightedElement());

    // Pointerover event over first option should not highlight first option,
    // since it follows a key event.
    option1.dispatchEvent(new PointerEvent('pointerover'));
    assertEquals(option2, getHighlightedElement());

    // Mock moving mouse within the first option again.
    option1.dispatchEvent(
        new PointerEvent('pointermove', {bubbles: true, composed: true}));
    assertEquals(option1, getHighlightedElement());
  });

  test('SelectsItem', async () => {
    combobox.items = [
      {
        label: 'Group A',
        items: [{label: 'I am option 1'}, {label: 'I am option 2'}],
      },
    ];
    await flushTasks();
    toggleGroupExpand(0);
    await flushTasks();

    const groupA = getGroup(0);
    const optionA1 = getOptionFromGroup(0, 0);
    const optionA2 = getOptionFromGroup(0, 1);

    // Open dropdown, click on first option to select it.
    combobox.$.input.click();
    optionA1.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    assertTrue(optionA1.hasAttribute('selected'));
    assertEquals('true', optionA1.ariaSelected);
    assertFalse(isVisible(combobox.$.dropdown));
    assertTrue(combobox.$.input.textContent!.includes('I am option 1'));

    // Open the dropdown back and arrow key to next option and select it.
    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertFalse(optionA2.hasAttribute('selected'));
    assertEquals('false', optionA2.ariaSelected);
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertTrue(optionA2.hasAttribute('selected'));
    assertEquals('true', optionA2.ariaSelected);
    assertFalse(optionA1.hasAttribute('selected'));
    assertEquals('false', optionA1.ariaSelected);
    assertTrue(combobox.$.input.textContent!.includes('I am option 2'));
    assertFalse(isVisible(combobox.$.dropdown));

    // Pressing Enter or clicking on an unselectable item should not select it.
    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Home'}));
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    const groupAClickEvent = eventToPromise('click', groupA);
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await groupAClickEvent;
    assertFalse(groupA.hasAttribute('selected'));
    groupA.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    assertFalse(groupA.hasAttribute('selected'));
    assertTrue(optionA2.hasAttribute('selected'));
    assertEquals('true', optionA2.ariaSelected);
    assertTrue(isVisible(combobox.$.dropdown));
  });

  test('NotifiesValueChange', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);

    let valueChangeEvent = eventToPromise('value-changed', combobox);
    combobox.$.input.click();
    option1.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    await valueChangeEvent;
    assertEquals('Option 1', combobox.value);
    assertTrue(option1.hasAttribute('selected'));

    valueChangeEvent = eventToPromise('value-changed', combobox);
    combobox.$.input.click();
    option2.dispatchEvent(new Event('click', {composed: true, bubbles: true}));
    assertEquals('Option 2', combobox.value);
    assertTrue(option2.hasAttribute('selected'));
  });

  test('UpdatesWithBoundValue', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);

    combobox.value = 'Option 1';
    await waitAfterNextRender(combobox);
    assertTrue(option1.hasAttribute('selected'));
    assertFalse(option2.hasAttribute('selected'));

    combobox.value = 'Option 2';
    await waitAfterNextRender(combobox);
    assertFalse(option1.hasAttribute('selected'));
    assertTrue(option2.hasAttribute('selected'));
  });

  test('SetsUniqueIdsAndAriaActiveDescendant', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);
    await flushTasks();

    assertTrue(option1.id.includes('comboboxItem'));
    assertTrue(option2.id.includes('comboboxItem'));
    assertNotEquals(option1.id, option2.id);

    combobox.$.input.click();
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        'defaultOption',
        combobox.$.input.getAttribute('aria-activedescendant'));

    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        option1.id, combobox.$.input.getAttribute('aria-activedescendant'));

    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(
        option2.id, combobox.$.input.getAttribute('aria-activedescendant'));
  });

  test('ExpandsAndCollapsesCategories', async () => {
    combobox.items = [
      {
        label: 'Group A',
        items: [{label: 'I am option 1'}, {label: 'I am option 2'}],
      },
    ];
    await flushTasks();

    // Only the default option should be visible yet since group is by default
    // collapsed.
    assertEquals(
        1, combobox.shadowRoot!.querySelectorAll('[role=option]').length);

    const groupLabel = getGroup(0).querySelector('label')!;
    const groupLabelIcon = groupLabel.querySelector('iron-icon')!;
    assertEquals('false', groupLabel.ariaExpanded);
    assertEquals('cr:expand-more', groupLabelIcon.icon);

    // // Clicking on a group expands the dropdown items below it.
    toggleGroupExpand(0);
    await flushTasks();
    assertEquals(
        3, combobox.shadowRoot!.querySelectorAll('[role=option]').length);
    assertEquals('true', groupLabel.ariaExpanded);
    assertEquals('cr:expand-less', groupLabelIcon.icon);

    // // Clicking on the group again hides the dropdown items below it.
    toggleGroupExpand(0);
    await flushTasks();
    assertEquals(
        1, combobox.shadowRoot!.querySelectorAll('[role=option]').length);
    assertEquals('false', groupLabel.ariaExpanded);
    assertEquals('cr:expand-more', groupLabelIcon.icon);
  });

  test('CheckmarksSelectedOption', async () => {
    combobox.items = [
      {label: 'Option 1', imagePath: 'image/path1.png'},
      {label: 'Option 2', imagePath: 'image/path2.png'},
    ];
    await flushTasks();

    const optionCheckmarks = combobox.shadowRoot!.querySelectorAll(
        'customize-chrome-check-mark-wrapper');
    assertEquals(2, optionCheckmarks.length);

    const option1Checkmark = optionCheckmarks[0]!;
    const option2Checkmark = optionCheckmarks[1]!;
    assertFalse(option1Checkmark.checked);
    assertFalse(option2Checkmark.checked);

    combobox.value = 'Option 1';
    await flushTasks();
    assertTrue(option1Checkmark.checked);
    assertFalse(option2Checkmark.checked);

    combobox.value = 'Option 2';
    await flushTasks();
    assertFalse(option1Checkmark.checked);
    assertTrue(option2Checkmark.checked);
  });

  test('SelectingDefaultOptionResetsValue', async () => {
    combobox.$.input.click();
    getOption(0).dispatchEvent(
        new Event('click', {composed: true, bubbles: true}));
    await flushTasks();
    assertEquals('Option 1', combobox.value);

    getDefaultOption().dispatchEvent(
        new Event('click', {composed: true, bubbles: true}));
    await flushTasks();
    assertEquals(undefined, combobox.value);
    assertEquals('true', getDefaultOption().getAttribute('aria-selected'));
  });

  test('IndentsDefaultOption', async () => {
    const defaultOptionStyles = window.getComputedStyle(getDefaultOption());
    assertEquals('44px', defaultOptionStyles.paddingInlineStart);

    // Groups should not indent default option.
    combobox.items = [
      {
        label: 'Group A',
        items: [{label: 'I am option 1'}, {label: 'I am option 2'}],
      },
    ];
    await flushTasks();
    assertEquals('20px', defaultOptionStyles.paddingInlineStart);

    // Items with images should not indent.
    combobox.items = [
      {label: 'Option 1', imagePath: 'image/path1.png'},
    ];
    await flushTasks();
    assertEquals('20px', defaultOptionStyles.paddingInlineStart);
  });
});
