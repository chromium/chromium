// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/combobox/customize_chrome_combobox.js';

import type {CustomizeChromeComboboxElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search/combobox/customize_chrome_combobox.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ComboboxTest', () => {
  let combobox: CustomizeChromeComboboxElement;

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
    getGroup(groupIndex).querySelector('label')!.click();
  }

  function getHighlightedElement() {
    return combobox.shadowRoot!.querySelector('[highlighted]');
  }

  function open() {
    combobox.$.input.click();
    return microtasksFinished();
  }

  function keydown(key: string) {
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key}));
    return microtasksFinished();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    combobox = document.createElement('customize-chrome-combobox');
    combobox.label = 'Label';
    combobox.defaultOptionLabel = 'Select a option';
    combobox.items = [
      {key: 'Key 1', label: 'Option 1'},
      {key: 'Key 2', label: 'Option 2'},
    ];
    document.body.appendChild(combobox);
    return microtasksFinished();
  });

  test('ShowsAndHides', async () => {
    assertFalse(isVisible(combobox.$.dropdown));
    await open();
    assertTrue(isVisible(combobox.$.dropdown));

    combobox.$.input.dispatchEvent(new FocusEvent('focusout'));
    await microtasksFinished();
    assertFalse(isVisible(combobox.$.dropdown));
  });

  test('OpensAndClosesDropdownOnKeydown', async () => {
    async function assertDropdownOpensAndHighlightsFirst(
        key: string, expectedHighlight: HTMLElement) {
      await keydown(key);
      assertTrue(isVisible(combobox.$.dropdown));
      assertEquals(
          expectedHighlight,
          combobox.shadowRoot!.querySelector('[highlighted]'));
      // Close the dropdown.
      return keydown('Escape');
    }

    await microtasksFinished();
    await assertDropdownOpensAndHighlightsFirst(
        'ArrowDown', getDefaultOption());
    await assertDropdownOpensAndHighlightsFirst('ArrowUp', getOption(1));
    await assertDropdownOpensAndHighlightsFirst('Home', getDefaultOption());
    await assertDropdownOpensAndHighlightsFirst('End', getOption(1));
    await assertDropdownOpensAndHighlightsFirst('Enter', getDefaultOption());
    await assertDropdownOpensAndHighlightsFirst('Space', getDefaultOption());
  });

  test('HighlightsItemsOnKeydownWhenOpen', async () => {
    combobox.items = [
      {
        key: 'Key A',
        label: 'Group A',
        items: [
          {key: 'Key A1', label: 'Option A1'},
          {key: 'KeyA2', label: 'OptionA2'},
        ],
      },
      {
        key: 'Key B',
        label: 'Group B',
        items: [{key: 'Key B1', label: 'Option B1'}],
      },
    ];
    await microtasksFinished();

    toggleGroupExpand(0);
    toggleGroupExpand(1);
    await microtasksFinished();

    const groupA = getGroup(0);
    const optionA1 = getOptionFromGroup(0, 0);
    const optionA2 = getOptionFromGroup(0, 1);
    const groupB = getGroup(1);
    const optionB1 = getOptionFromGroup(1, 0);

    // ArrowDown should loop through list.
    await open();
    await keydown('ArrowDown');
    assertEquals(getDefaultOption(), getHighlightedElement());
    await keydown('ArrowDown');
    assertEquals(groupA.querySelector('label'), getHighlightedElement());
    await keydown('ArrowDown');
    assertEquals(optionA1, getHighlightedElement());
    await keydown('ArrowDown');
    assertEquals(optionA2, getHighlightedElement());
    await keydown('ArrowDown');
    assertEquals(groupB.querySelector('label'), getHighlightedElement());
    await keydown('ArrowDown');
    assertEquals(optionB1, getHighlightedElement());
    await keydown('ArrowDown');
    assertEquals(getDefaultOption(), getHighlightedElement());

    // ArrowUp goes reverse order.
    await keydown('ArrowUp');
    assertEquals(optionB1, getHighlightedElement());

    // Home and End keys work.
    await keydown('Home');
    assertEquals(getDefaultOption(), getHighlightedElement());
    await keydown('End');
    assertEquals(optionB1, getHighlightedElement());

    // Closes when hitting Escape and resets highlight.
    await keydown('Escape');
    assertFalse(isVisible(combobox.$.dropdown));
    assertEquals(null, getHighlightedElement());
  });

  test('HighlightsOnPointerover', async () => {
    combobox.items = [
      {
        key: 'Key A',
        label: 'Group A',
        items: [{key: 'Key A1', label: 'Option A1'}],
      },
    ];
    await microtasksFinished();
    toggleGroupExpand(0);
    await microtasksFinished();

    const group = getGroup(0);
    const option = getOptionFromGroup(0, 0);

    // Open the dropdown.
    await open();

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
    await open();

    // Mouse moves to first option.
    option1.dispatchEvent(
        new PointerEvent('pointerover', {bubbles: true, composed: true}));
    assertEquals(option1, getHighlightedElement());

    // Keydown down to highlight second option. Mouse still over first option.
    await keydown('ArrowDown');
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
        key: 'Key A',
        label: 'Group A',
        items: [
          {key: 'I am key 1', label: 'I am option 1'},
          {key: 'I am key 2', label: 'I am option 2'},
        ],
      },
    ];
    await microtasksFinished();
    toggleGroupExpand(0);
    await microtasksFinished();

    const groupA = getGroup(0);
    const optionA1 = getOptionFromGroup(0, 0);
    let optionA2 = getOptionFromGroup(0, 1);

    // Open dropdown, click on first option to select it.
    await open();
    optionA1.click();
    await microtasksFinished();
    assertTrue(optionA1.hasAttribute('selected'));
    assertEquals('true', optionA1.ariaSelected);
    assertFalse(isVisible(combobox.$.dropdown));
    assertTrue(combobox.$.input.textContent!.includes('I am option 1'));

    // Open the dropdown back and arrow key to next option and select it.
    await open();
    await keydown('ArrowDown');
    assertFalse(optionA2.hasAttribute('selected'));
    assertEquals('false', optionA2.ariaSelected);
    await keydown('Enter');
    assertTrue(optionA2.hasAttribute('selected'));
    assertEquals('true', optionA2.ariaSelected);
    assertFalse(optionA1.hasAttribute('selected'));
    assertEquals('false', optionA1.ariaSelected);
    assertTrue(combobox.$.input.textContent!.includes('I am option 2'));
    assertFalse(isVisible(combobox.$.dropdown));

    // Pressing Enter or clicking on an unselectable item should not select it.
    await open();
    await keydown('Home');
    await keydown('ArrowDown');
    const groupAClickEvent = eventToPromise('click', groupA);
    await keydown('Enter');
    await groupAClickEvent;
    assertFalse(groupA.hasAttribute('selected'));
    groupA.click();
    await microtasksFinished();
    assertFalse(groupA.hasAttribute('selected'));

    // Need to re-query since `optionA2` from line 219 is no longer in the DOM,
    // after Lit re-renders and a new DOM node is created.
    optionA2 = getOptionFromGroup(0, 1);
    assertTrue(optionA2.hasAttribute('selected'));
    assertEquals('true', optionA2.ariaSelected);
    assertTrue(isVisible(combobox.$.dropdown));
  });

  test('UnselectsItems', async () => {
    await open();
    const option = getOption(0);

    // Clicking and re-clicking should unselect item.
    option.click();
    await microtasksFinished();
    assertTrue(option.hasAttribute('selected'));
    assertEquals('Key 1', combobox.value);
    option.click();
    await microtasksFinished();
    assertFalse(option.hasAttribute('selected'));
    assertEquals(undefined, combobox.value);

    // Unselecting by keyboard should also work.
    await keydown('Home');
    await keydown('ArrowDown');
    await keydown('Enter');
    assertTrue(option.hasAttribute('selected'));
    assertEquals('Key 1', combobox.value);
    await open();
    await keydown('Enter');
    assertFalse(option.hasAttribute('selected'));
    assertEquals(undefined, combobox.value);
  });

  test('NotifiesValueChange', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);

    let valueChangeEvent = eventToPromise('value-changed', combobox);
    await open();
    option1.click();
    await valueChangeEvent;
    assertEquals('Key 1', combobox.value);
    assertTrue(option1.hasAttribute('selected'));

    valueChangeEvent = eventToPromise('value-changed', combobox);
    await open();
    option2.click();
    await valueChangeEvent;
    assertEquals('Key 2', combobox.value);
    assertTrue(option2.hasAttribute('selected'));
  });

  test('UpdatesWithBoundValue', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);

    combobox.value = 'Key 1';
    await microtasksFinished();
    assertTrue(option1.hasAttribute('selected'));
    assertFalse(option2.hasAttribute('selected'));

    combobox.value = 'Key 2';
    await microtasksFinished();
    assertFalse(option1.hasAttribute('selected'));
    assertTrue(option2.hasAttribute('selected'));
  });

  test('SetsUniqueIdsAndAriaActiveDescendant', async () => {
    const option1 = getOption(0);
    const option2 = getOption(1);

    assertTrue(option1.id.includes('comboboxItem'));
    assertTrue(option2.id.includes('comboboxItem'));
    assertNotEquals(option1.id, option2.id);

    await open();
    await keydown('ArrowDown');
    assertEquals(
        'defaultOption',
        combobox.$.input.getAttribute('aria-activedescendant'));

    await keydown('ArrowDown');
    assertEquals(
        option1.id, combobox.$.input.getAttribute('aria-activedescendant'));

    await keydown('ArrowDown');
    assertEquals(
        option2.id, combobox.$.input.getAttribute('aria-activedescendant'));
  });

  test('ExpandsAndCollapsesCategories', async () => {
    combobox.items = [
      {
        key: 'Key A',
        label: 'Group A',
        items: [
          {key: 'I am key 1', label: 'I am option 1'},
          {key: 'I am key 2', label: 'I am option 2'},
        ],
      },
    ];
    await open();

    // Only the default option should be visible yet since group is by default
    // collapsed.
    assertEquals(
        1, combobox.shadowRoot!.querySelectorAll('[role=option]').length);

    const groupLabel = getGroup(0).querySelector('label')!;
    const groupLabelIcon = groupLabel.querySelector('cr-icon')!;
    assertEquals('false', groupLabel.ariaExpanded);
    assertEquals('cr:expand-more', groupLabelIcon.icon);

    // Clicking on a group expands the dropdown items below it.
    toggleGroupExpand(0);
    await microtasksFinished();
    const options = Array.from<HTMLElement>(
        combobox.shadowRoot!.querySelectorAll('[role=option]'));
    assertEquals(3, options.filter(option => isVisible(option)).length);

    assertEquals('true', groupLabel.ariaExpanded);
    assertEquals('cr:expand-less', groupLabelIcon.icon);

    // Clicking on the group again hides the dropdown items below it.
    toggleGroupExpand(0);
    await microtasksFinished();
    assertEquals(1, options.filter(option => isVisible(option)).length);
    assertEquals('false', groupLabel.ariaExpanded);
    assertEquals('cr:expand-more', groupLabelIcon.icon);
  });

  test('CheckmarksSelectedOption', async () => {
    combobox.items = [
      {key: 'Key 1', label: 'Option 1', imagePath: 'image/path1.png'},
      {key: 'Key 2', label: 'Option 2', imagePath: 'image/path2.png'},
    ];
    await microtasksFinished();

    const optionCheckmarks = combobox.shadowRoot!.querySelectorAll(
        'customize-chrome-check-mark-wrapper');
    assertEquals(2, optionCheckmarks.length);

    const option1Checkmark = optionCheckmarks[0]!;
    const option2Checkmark = optionCheckmarks[1]!;
    assertFalse(option1Checkmark.checked);
    assertFalse(option2Checkmark.checked);

    combobox.value = 'Key 1';
    await microtasksFinished();
    assertTrue(option1Checkmark.checked);
    assertFalse(option2Checkmark.checked);

    combobox.value = 'Key 2';
    await microtasksFinished();
    assertFalse(option1Checkmark.checked);
    assertTrue(option2Checkmark.checked);
  });

  test('SelectingDefaultOptionResetsValue', async () => {
    await open();
    getOption(0).click();
    await microtasksFinished();
    assertEquals('Key 1', combobox.value);

    getDefaultOption().click();
    await microtasksFinished();
    assertEquals(undefined, combobox.value);
    assertEquals('true', getDefaultOption().getAttribute('aria-selected'));
  });

  test('IndentsDefaultOption', async () => {
    const defaultOptionStyles = window.getComputedStyle(getDefaultOption());
    assertEquals('44px', defaultOptionStyles.paddingInlineStart);

    // Groups should not indent default option.
    combobox.items = [
      {
        key: 'Key A',
        label: 'Group A',
        items: [
          {key: 'I am key 1', label: 'I am option 1'},
          {key: 'I am key 2', label: 'I am option 2'},
        ],
      },
    ];
    await microtasksFinished();
    assertEquals('20px', defaultOptionStyles.paddingInlineStart);

    // Items with images should not indent.
    combobox.items = [
      {key: 'Key 1', label: 'Option 1', imagePath: 'image/path1.png'},
    ];
    await microtasksFinished();
    assertEquals('20px', defaultOptionStyles.paddingInlineStart);
  });
});
