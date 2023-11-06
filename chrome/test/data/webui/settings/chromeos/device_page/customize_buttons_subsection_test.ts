// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonsSubsectionElement, KeyCombinationInputDialogElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets} from 'chrome://os-settings/os_settings.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-buttons-subsection>', () => {
  let customizeButtonsSubsection: CustomizeButtonsSubsectionElement;
  let buttonRemappingChangedEventCount: number = 0;
  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!customizeButtonsSubsection) {
      return;
    }
    buttonRemappingChangedEventCount = 0;
    customizeButtonsSubsection.remove();
    await flushTasks();
  });

  async function initializeCustomizeButtonsSubsection() {
    customizeButtonsSubsection =
        document.createElement(CustomizeButtonsSubsectionElement.is);
    customizeButtonsSubsection.addEventListener(
        'button-remapping-changed', function() {
          buttonRemappingChangedEventCount++;
        });
    customizeButtonsSubsection.set(
        'actionList', fakeGraphicsTabletButtonActions);
    customizeButtonsSubsection.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings!.penButtonRemappings);
    document.body.appendChild(customizeButtonsSubsection);
    return await flushTasks();
  }

  test('Initialize customize buttons subsection', async () => {
    await initializeCustomizeButtonsSubsection();
    assertTrue(!!customizeButtonsSubsection);
    assertTrue(!!customizeButtonsSubsection.get('buttonRemappingList'));

    // Verify that renaming dialog will pop out when setting
    // shouldShowRenamingDialog_ to true.
    customizeButtonsSubsection.set('shouldShowRenamingDialog_', true);
    customizeButtonsSubsection.set(
        'selectedButton_',
        fakeGraphicsTablets[0]!.settings!.penButtonRemappings[0]);
    customizeButtonsSubsection.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings!.penButtonRemappings);
    await flushTasks();
    assertTrue(!!customizeButtonsSubsection.shadowRoot!.querySelector(
        '#renamingDialog'));

    // Verify that the renaming dialog update the button name after clicking
    // 'save' button.
    const buttonLabelInput: CrInputElement|null =
        customizeButtonsSubsection.shadowRoot!.querySelector(
            '#renamingDialogInput');
    const saveButton: CrButtonElement|null =
        customizeButtonsSubsection.shadowRoot!.querySelector('#saveButton');
    assertTrue(!!buttonLabelInput);
    assertEquals(buttonRemappingChangedEventCount, 0);
    assertTrue(!!saveButton);
    await flushTasks();

    // Verify that if the input name is empty, the save button is disabled.
    buttonLabelInput.value = '';
    await flushTasks();
    assertTrue(saveButton.disabled);

    // Verify that if the new button is too long, it will cause invalid and be
    // truncated.
    buttonLabelInput.value =
        'Button name which exceeds 32 character is invalid.';
    await flushTasks();

    assertFalse(saveButton.disabled);
    assertTrue(customizeButtonsSubsection.get('buttonNameInvalid_'));
    assertEquals(buttonLabelInput.value.length, 32);
    const inputCountText: HTMLDivElement|null =
        customizeButtonsSubsection.shadowRoot!.querySelector('#inputCount');
    assertEquals(inputCountText!.textContent!.trim(), '32/32');

    // Verify that if the button name is duplicate with other buttons, the
    // save button is blocked.
    buttonLabelInput.value = 'Redo';
    saveButton.click();
    await flushTasks();
    assertTrue(buttonLabelInput.invalid);
    assertEquals(buttonRemappingChangedEventCount, 0);

    buttonLabelInput.value = 'New Button Name';
    assertEquals(inputCountText!.textContent!.trim(), '15/32');
    assertFalse(customizeButtonsSubsection.get('buttonNameInvalid_'));
    saveButton.click();
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 1);
    assertFalse(customizeButtonsSubsection.get('shouldShowRenamingDialog_'));
    assertFalse(!!customizeButtonsSubsection.shadowRoot!.querySelector(
        '#renamingDialog'));
  });

  test('open key combination dialog', async () => {
    await initializeCustomizeButtonsSubsection();
    const keyCombinationDialog: KeyCombinationInputDialogElement|null =
        customizeButtonsSubsection.shadowRoot!.querySelector(
            '#keyCombinationInputDialog');
    assertTrue(!!keyCombinationDialog);
    assertFalse(keyCombinationDialog.isOpen);
    customizeButtonsSubsection.dispatchEvent(
        new CustomEvent('show-key-combination-dialog', {
          bubbles: true,
          composed: true,
          detail: {
            buttonIndex: 0,
          },
        }));
    await flushTasks();
    assertTrue(keyCombinationDialog.isOpen);
  });

  test('Drop event should trigger remapping', async () => {
    await initializeCustomizeButtonsSubsection();
    assertTrue(!!customizeButtonsSubsection);
    assertTrue(!!customizeButtonsSubsection.get('buttonRemappingList'));

    const buttonRemappingListBefore =
        structuredClone(customizeButtonsSubsection.buttonRemappingList);

    // Call the callback directly since the event listening functionality
    // is handled by DragAndDropManager (which is unit tested separately).
    // @ts-expect-error (we're invoking a private method for the test).
    customizeButtonsSubsection.onDrop_(1, 0);
    await flushTasks();

    assertNotEquals(
        buttonRemappingListBefore[0],
        customizeButtonsSubsection.buttonRemappingList[0]);
    assertNotEquals(
        buttonRemappingListBefore[1],
        customizeButtonsSubsection.buttonRemappingList[1]);

    const expectedRemappingList = buttonRemappingListBefore.slice();
    const secondItem = buttonRemappingListBefore[1]!;
    // Remove second item.
    expectedRemappingList.splice(1, 1);
    // Add second item at beginning.
    expectedRemappingList.splice(0, 0, secondItem);

    assertDeepEquals(
        expectedRemappingList, customizeButtonsSubsection.buttonRemappingList);

    assertEquals(1, buttonRemappingChangedEventCount);

    // When onDrop_ is called with invalid indices, the button remapping list
    // should not change.
    // @ts-expect-error (we're invoking a private method for the test).
    customizeButtonsSubsection.onDrop_(100, -10);
    await flushTasks();

    assertEquals(1, buttonRemappingChangedEventCount);
  });
});
