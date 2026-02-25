// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-shortcut-input. */

import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import 'chrome://extensions/strings.m.js';

import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {Modifier} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('CrShortcutInputTest', function() {
  let input: CrShortcutInputElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    input = document.createElement('cr-shortcut-input');
    document.body.appendChild(input);
  });

  async function assertError(
      isKeyDown: boolean, keyCode: number, modifiers: Modifier[],
      expectedErrorStringId: string) {
    const field = input.$.input;
    if (isKeyDown) {
      keyDownOn(field, keyCode, modifiers);
    } else {
      keyUpOn(field, keyCode, modifiers);
    }
    await microtasksFinished();
    assertEquals('', field.value);
    assertEquals(
        loadTimeData.getString(expectedErrorStringId), field.errorMessage);
  }

  async function activateInputCapture() {
    const whenInputCaptureChange =
        eventToPromise('input-capture-change', input);
    input.$.edit.click();
    const event = await whenInputCaptureChange;
    assertTrue(event.detail);
    await microtasksFinished();
  }

  test('Basic', async function() {
    const field = input.$.input;
    assertEquals('', field.value);

    // Click the edit button. Capture should start.
    await activateInputCapture();

    // Press character.
    await assertError(true, 65, [], 'shortcutIncludeStartModifier');
    // Add shift to character.
    await assertError(true, 65, ['shift'], 'shortcutIncludeStartModifier');
    // Press ctrl.
    await assertError(true, 17, ['ctrl'], 'shortcutNeedCharacter');
    // Add shift.
    await assertError(true, 17, ['ctrl', 'shift'], 'shortcutNeedCharacter');
    // Remove shift.
    await assertError(false, 17, ['ctrl'], 'shortcutNeedCharacter');

    // Add 'A'. Once a valid shortcut is typed (like Ctrl + A), it is
    // committed.
    const whenShortcutUpdate = eventToPromise('shortcut-updated', input);
    keyDownOn(field, 65, ['ctrl']);
    let event = await whenShortcutUpdate;
    assertEquals('Ctrl+A', event.detail);

    await microtasksFinished();
    assertEquals('Ctrl + A', field.value);
    assertEquals('Ctrl+A', input.shortcut);

    // Test that clicking edit preserves the last shortcut value.
    input.$.edit.click();
    await microtasksFinished();
    assertEquals('', field.value);
    assertEquals('Ctrl+A', input.shortcut);

    // Test that ending capture using the escape key restores the value.
    const stopInputCapturePromise =
        eventToPromise('input-capture-change', input);
    keyDownOn(field, 27);  // Escape key.
    await stopInputCapturePromise;
    assertEquals('Ctrl + A', field.value);
    assertEquals('Ctrl+A', input.shortcut);

    // Test that clicking clear erases the field value and the stored shortcut
    // value.
    const clearShortcutPromise = eventToPromise('shortcut-updated', input);
    input.$.clear.click();
    event = await clearShortcutPromise;
    await microtasksFinished();
    assertEquals('', event.detail);
    assertEquals('', input.shortcut);
    assertEquals('', field.value);

    // The `input-capture-change` event should happen once when the edit button
    // is clicked.
    const inputCaptureChangeResults: boolean[] = [];
    input.addEventListener('input-capture-change', (e) => {
      inputCaptureChangeResults.push((e as CustomEvent<boolean>).detail);
    });

    input.$.edit.click();
    await microtasksFinished();
    assertEquals(1, inputCaptureChangeResults.length);
    assertTrue(inputCaptureChangeResults[0]!);
  });

  test('ButtonDisabledStates', async function() {
    // Initially shortcut is empty - clear button should be disabled.
    assertTrue(input.$.clear.disabled);
    assertFalse(input.$.edit.disabled);

    // Set a shortcut.
    input.shortcut = 'Ctrl+A';
    await microtasksFinished();
    assertFalse(input.$.clear.disabled);
    assertFalse(input.$.edit.disabled);

    // Start editing. During editing, the edit button should be disabled.
    input.$.edit.click();
    await microtasksFinished();
    assertFalse(input.$.clear.disabled);
    assertTrue(input.$.edit.disabled);

    // Clear shortcut - the clear button should return to being disabled.
    input.$.clear.click();
    await microtasksFinished();
    assertTrue(input.$.clear.disabled);
    assertFalse(input.$.edit.disabled);
  });

  test('AriaLabelUpdates', async function() {
    // Verify that the aria labels are initially empty.
    assertEquals('', input.$.input.ariaLabel);
    assertEquals('', input.$.edit.ariaLabel);
    assertEquals('', input.$.clear.ariaLabel);

    // Update the aria labels.
    const inputAriaLabel = 'input';
    const editButtonAriaLabel = 'edit';
    const clearButtonAriaLabel = 'clear';
    input.inputAriaLabel = inputAriaLabel;
    input.editButtonAriaLabel = editButtonAriaLabel;
    input.clearButtonAriaLabel = clearButtonAriaLabel;
    await microtasksFinished();
    assertEquals(inputAriaLabel, input.$.input.ariaLabel);
    assertEquals(editButtonAriaLabel, input.$.edit.ariaLabel);
    assertEquals(clearButtonAriaLabel, input.$.clear.ariaLabel);
  });

  test('GetBubbleAnchor', function() {
    assertNotEquals(input.getBubbleAnchor(), null);
    assertEquals(input.$.edit, input.getBubbleAnchor());
  });

  test('allowCtrlAltShortcuts_CtrlAlt', async function() {
    assertFalse(input.allowCtrlAltShortcuts);
    activateInputCapture();

    // Press Ctrl + Alt, which should be invalid.
    await assertError(true, 17, ['ctrl', 'alt'], 'shortcutTooManyModifiers');
    // Remove alt.
    await assertError(false, 17, ['ctrl'], 'shortcutNeedCharacter');

    input.allowCtrlAltShortcuts = true;
    const field = input.$.input;
    keyDownOn(field, 65, ['ctrl', 'alt']);
    await microtasksFinished();
    assertEquals('Ctrl + Alt + A', field.value);
    assertEquals('Ctrl+Alt+A', input.shortcut);
  });

  // <if expr="is_macosx">
  test('allowCtrlAltShortcuts_CommandAlt', async function() {
    assertFalse(input.allowCtrlAltShortcuts);
    activateInputCapture();

    // Press Command + Alt, which should be invalid.
    await assertError(true, 65, ['meta', 'alt'], 'shortcutTooManyModifiers');
    // Remove alt.
    await assertError(false, 17, ['meta'], 'shortcutNeedCharacter');

    input.allowCtrlAltShortcuts = true;
    const field = input.$.input;
    keyDownOn(field, 65, ['meta', 'alt']);
    await microtasksFinished();
    assertEquals('Command + Alt + A', field.value);
    assertEquals('Command+Alt+A', input.shortcut);
  });

  test('allowCtrlAltShortcuts_AllModifiers', async function() {
    input.allowCtrlAltShortcuts = true;
    activateInputCapture();

    // Press Command + Alt + Ctrl + Shift, which should be invalid.
    await assertError(
        true, 65, ['meta', 'alt', 'ctrl', 'shift'], 'shortcutTooManyModifiers');
    // Remove alt.
    await assertError(
        false, 17, ['meta', 'ctrl', 'shift'], 'shortcutNeedCharacter');
  });
  // </if>
});
