// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonSelectElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-select>', () => {
  let select: CustomizeButtonSelectElement;
  let buttonRemappingChangedEventCount: number = 0;
  let showKeyCombinationDialogEventCount: number = 0;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!select) {
      return;
    }
    select.remove();
    showKeyCombinationDialogEventCount = 0;
    buttonRemappingChangedEventCount = 0;
    await flushTasks();
  });

  async function initializeSelect() {
    select = document.createElement(CustomizeButtonSelectElement.is);

    select.set('actionList', fakeGraphicsTabletButtonActions);
    select.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings);
    select.set('remappingIndex', 0);
    await flushTasks();

    select.addEventListener('button-remapping-changed', function() {
      buttonRemappingChangedEventCount++;
    });
    select.addEventListener('show-key-combination-dialog', function() {
      showKeyCombinationDialogEventCount++;
    });
    document.body.appendChild(select);
    select.blur();
    return flushTasks();
  }

  function getSelectedValue(): string {
    assertTrue(!!select);
    return select!.get('selectedValue');
  }

  test('Initialize customize button select', async () => {
    await initializeSelect();

    assertTrue(!!select);
    let expectedRemapping =
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings[0];
    assertEquals(
        select.get('menu')?.length, fakeGraphicsTabletButtonActions.length + 4);
    assertEquals(select.get('label_'), 'Back');
    assertEquals(
        getSelectedValue(),
        'acceleratorAction' +
            expectedRemapping!.remappingAction?.acceleratorAction!.toString());

    // Change buttonRemapping data to display.
    select.set('remappingIndex', 1);
    select.set(
        'buttonRemappingList',
        fakeGraphicsTablets[1]!.settings.tabletButtonRemappings);
    await flushTasks();
    expectedRemapping =
        fakeGraphicsTablets[1]!.settings.tabletButtonRemappings[1];
    assertEquals(select.get('label_'), expectedRemapping!.name);
    assertEquals(
        getSelectedValue(),
        'acceleratorAction' +
            expectedRemapping!.remappingAction?.acceleratorAction!.toString());
  });

  test('Initialize key combination string', async () => {
    await initializeSelect();
    select.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.penButtonRemappings);
    select.set('remappingIndex', 0);
    await flushTasks();

    assertEquals(getSelectedValue(), 'key combination');
    assertTrue(select.get('remappedToKeyCombination_'));
    assertEquals(select.get('label_'), 'Create key combination');
    assertDeepEquals(select.get('inputKeys_'), ['ctrl', '+', 'z']);

    // Switch to another button remapping.
    select.set(
        'buttonRemappingList',
        fakeGraphicsTablets[1]!.settings.penButtonRemappings);
    select.set('remappingIndex', 1);
    await flushTasks();

    assertEquals(getSelectedValue(), 'key combination');
    assertTrue(select.get('remappedToKeyCombination_'));
    assertEquals(select.get('label_'), 'Create key combination');
    assertDeepEquals(select.get('inputKeys_'), ['ctrl', '+', 'v']);
  });

  test('update dropdown will sent events', async () => {
    await initializeSelect();
    buttonRemappingChangedEventCount = 0;
    assertEquals(getSelectedValue(), 'acceleratorAction2');
    // Update select to another remapping action.
    select.selectedValue = 'acceleratorAction1';
    await flushTasks();

    // Verify that event is fired and button remapping is updated.
    assertEquals(buttonRemappingChangedEventCount, 1);
    assertDeepEquals(select.get('buttonRemapping_')?.remappingAction, {
      acceleratorAction: 1,
    });

    // Update select to no remapping action choice.
    select.selectedValue = 'none';
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 2);
    assertEquals(select.get('buttonRemapping_')?.remappingAction, null);

    // Update select from no remapping back to normal remapping action.
    select.selectedValue = 'acceleratorAction2';
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 3);
    assertDeepEquals(select.get('buttonRemapping_')?.remappingAction, {
      acceleratorAction: 2,
    });

    // Update select to the same action, no events will be fired.
    select.selectedValue = 'acceleratorAction2';
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 3);
  });

  test('select listens for dropdown selected event', async () => {
    await initializeSelect();

    assertEquals(showKeyCombinationDialogEventCount, 0);

    select.dispatchEvent(new CustomEvent('customize-button-dropdown-selected', {
      bubbles: true,
      composed: true,
      detail: {
        value: 'open key combination dialog',
      },
    }));

    await flushTasks();
    assertEquals(showKeyCombinationDialogEventCount, 1);
    // Verify that the selected value will change back to
    // the previous selection.
    assertEquals(select.selectedValue, 'acceleratorAction2');

    // Verify that when clicking the open key combination value again,
    // the open dialog event will fire again.
    select.dispatchEvent(new CustomEvent('customize-button-dropdown-selected', {
      bubbles: true,
      composed: true,
      detail: {
        value: 'open key combination dialog',
      },
    }));

    await flushTasks();
    assertEquals(showKeyCombinationDialogEventCount, 2);
    assertEquals(select.selectedValue, 'acceleratorAction2');

    // Verify that when the dropdown was selected to other actions,
    // selectedValue will update.
    buttonRemappingChangedEventCount = 0;
    select.dispatchEvent(new CustomEvent('customize-button-dropdown-selected', {
      bubbles: true,
      composed: true,
      detail: {
        value: 'none',
      },
    }));

    await flushTasks();
    assertEquals(select.selectedValue, 'none');
    assertEquals(buttonRemappingChangedEventCount, 1);
  });

  test('select react to key event', async () => {
    await initializeSelect();
    assertFalse(select.get('shouldShowDropdownMenu_'));
    assertEquals(buttonRemappingChangedEventCount, 0);

    const enterEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'Enter', keyCode: 13});
    select.dispatchEvent(enterEvent);

    await flushTasks();
    assertTrue(select.get('shouldShowDropdownMenu_'));

    // Value of "no remapping" should be selected and highlighted.
    assertEquals(select.get('selectedValue'), 'none');
    assertEquals(select.get('highlightedValue_'), 'none');
    select.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'ArrowDown', keyCode: 40},
        ));

    // Value of kBrightnessDown should be highlighted.
    assertEquals(select.get('selectedValue'), 'none');
    assertEquals(select.get('highlightedValue_'), 'acceleratorAction0');

    select.dispatchEvent(new KeyboardEvent(
        'keydown', {cancelable: true, key: 'Enter', keyCode: 13}));

    // Value of kBrightnessDown should be selected.
    assertEquals(select.get('selectedValue'), 'acceleratorAction0');
    assertEquals(buttonRemappingChangedEventCount, 1);
    assertFalse(select.get('shouldShowDropdownMenu_'));
  });
});
