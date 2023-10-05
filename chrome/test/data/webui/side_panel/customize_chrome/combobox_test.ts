// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/combobox/customize_chrome_combobox.js';

import {CustomizeChromeCombobox} from 'chrome://customize-chrome-side-panel.top-chrome/combobox/customize_chrome_combobox.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ComboboxTest', () => {
  let combobox: CustomizeChromeCombobox;

  function addGroup(): HTMLElement {
    const group = document.createElement('div');
    group.setAttribute('role', 'group');
    combobox.appendChild(group);
    return group;
  }

  function addOption(parent: HTMLElement = combobox): HTMLElement {
    const option = document.createElement('div');
    option.setAttribute('role', 'option');
    option.innerText = 'Option';
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

  test('HighlightsItemsOnKeydown', async () => {
    const groupA = addGroup();
    const optionA1 = addOption(groupA);
    const optionA2 = addOption(groupA);
    const groupB = addGroup();
    const optionB1 = addOption(groupB);
    await flushTasks();

    // In collapsed state, should not highlight anything.
    combobox.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(null, combobox.querySelector('[highlighted]'));

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

    // Resets highlighted item when collapsing.
    combobox.$.input.dispatchEvent(new FocusEvent('focusout'));
    assertEquals(null, combobox.querySelector('[highlighted]'));
  });
});
