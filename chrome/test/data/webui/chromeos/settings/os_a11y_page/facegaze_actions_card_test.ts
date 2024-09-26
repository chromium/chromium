// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AddDialogPage, AssignedKeyCombo, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeActionsCardElement, FaceGazeAddActionDialogElement, FaceGazeCommandPair, KeyCombination} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrIconButtonElement, CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<facegaze-actions-card>', () => {
  function getDialog(): FaceGazeAddActionDialogElement {
    const dialog =
        faceGazeActionsCard.shadowRoot!
            .querySelector<FaceGazeAddActionDialogElement>('#actionsAddDialog');
    assertTrue(!!dialog);
    return dialog;
  }

  function getAddButton(): CrButtonElement {
    return getButton('#addActionButton');
  }

  function getButton(id: string): CrButtonElement {
    const button =
        faceGazeActionsCard.shadowRoot!.querySelector<CrButtonElement>(id);
    assertTrue(!!button);
    assertTrue(isVisible(button));
    return button;
  }

  function assertActionSettingsRow(commandPair: FaceGazeCommandPair):
      HTMLDivElement {
    const domRepeat =
        faceGazeActionsCard.shadowRoot!.querySelector('dom-repeat');
    assertTrue(!!domRepeat);
    const settingsRows =
        faceGazeActionsCard.shadowRoot!.querySelectorAll<HTMLDivElement>(
            '.settings-box');
    const row =
        Array.from(settingsRows.values())
            .find(
                (row) => domRepeat.itemForElement(row)!.equals(commandPair)) as
        HTMLDivElement;
    assertTrue(!!row);
    return row;
  }

  let faceGazeActionsCard: FaceGazeActionsCardElement;
  let prefElement: SettingsPrefsElement;

  function isGestureToMacroPrefSet(expectedCommandPair: FaceGazeCommandPair):
      boolean {
    if (!expectedCommandPair.gesture) {
      return false;
    }

    const assignedGestures = {...faceGazeActionsCard.prefs.settings.a11y
                                  .face_gaze.gestures_to_macros.value};
    return assignedGestures[expectedCommandPair.gesture] ===
        expectedCommandPair.action;
  }

  function isGestureToKeyComboPrefSet(expectedCommandPair: FaceGazeCommandPair):
      boolean {
    if (!expectedCommandPair.gesture || !expectedCommandPair.assignedKeyCombo) {
      return false;
    }

    const assignedKeyCombos: Record<FacialGesture, string> = {
        ...faceGazeActionsCard.prefs.settings.a11y.face_gaze
            .gestures_to_key_combos.value};
    return assignedKeyCombos[expectedCommandPair.gesture] ===
        expectedCommandPair.assignedKeyCombo.prefString;
  }

  async function openAddDialogAndFireCommandPairAddedEvent(
      commandPair: FaceGazeCommandPair) {
    await openDialogAndFireCommandPairAddedEvent(getAddButton(), commandPair);
  }

  async function openDialogAndFireCommandPairAddedEvent(
      trigger: HTMLElement, commandPair: FaceGazeCommandPair) {
    trigger.click();
    await flushTasks();

    const dialog = getDialog();

    const event = new CustomEvent(FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, {
      bubbles: true,
      composed: true,
      detail: commandPair,
    });

    dialog.dispatchEvent(event);
  }

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeActionsCard = document.createElement('facegaze-actions-card');
    faceGazeActionsCard.prefs = prefElement.prefs;
    document.body.appendChild(faceGazeActionsCard);
    flush();
  }

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    faceGazeActionsCard.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('actions enabled button syncs to pref', async () => {
    await initPage();
    assertTrue(faceGazeActionsCard.prefs.settings.a11y.face_gaze.actions_enabled
                   .value);

    const button = faceGazeActionsCard.shadowRoot!
                       .querySelector<SettingsToggleButtonElement>(
                           '#faceGazeActionsEnabledButton');
    assert(button);
    assertTrue(isVisible(button));
    assertTrue(button.checked);

    button.click();
    flush();

    assertFalse(button.checked);
    assertFalse(faceGazeActionsCard.prefs.settings.a11y.face_gaze
                    .actions_enabled.value);
  });

  test('actions disables controls if feature is disabled', async () => {
    await initPage();

    faceGazeActionsCard.disabled = true;
    await flushTasks();

    const addButton = getAddButton();
    assertTrue(addButton.disabled);

    faceGazeActionsCard.disabled = false;
    await flushTasks();
    assertFalse(addButton.disabled);
  });

  test(
      'actions disables configuration controls if toggle is turned off',
      async () => {
        await initPage();

        faceGazeActionsCard.set(
            'prefs.settings.a11y.face_gaze.actions_enabled.value', true);
        await flushTasks();

        const addButton = getAddButton();
        assertFalse(addButton.disabled);

        faceGazeActionsCard.set(
            'prefs.settings.a11y.face_gaze.actions_enabled.value', false);
        await flushTasks();

        assertTrue(addButton.disabled);
      });

  test('actions initializes command pairs from prefs', async () => {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeActionsCard = document.createElement('facegaze-actions-card');
    faceGazeActionsCard.prefs = prefElement.prefs;

    const expectedMacro: MacroName = MacroName.MOUSE_CLICK_LEFT;
    const expectedGesture: FacialGesture = FacialGesture.EYES_BLINK;
    faceGazeActionsCard.prefs.settings.a11y.face_gaze.gestures_to_macros
        .value[expectedGesture] = expectedMacro;

    const keyComboGesture: FacialGesture = FacialGesture.BROW_INNER_UP;
    const keyCombo: KeyCombination = {
      key: 67,
      keyDisplay: 'c',
      modifiers: {
        ctrl: true,
      },
    };
    const keyComboPrefString = JSON.stringify(keyCombo);
    faceGazeActionsCard.prefs.settings.a11y.face_gaze.gestures_to_macros
        .value[keyComboGesture] = MacroName.CUSTOM_KEY_COMBINATION;
    faceGazeActionsCard.prefs.settings.a11y.face_gaze.gestures_to_key_combos
        .value[keyComboGesture] = keyComboPrefString;
    const keyComboCommandPair = new FaceGazeCommandPair(
        MacroName.CUSTOM_KEY_COMBINATION, keyComboGesture);
    keyComboCommandPair.assignedKeyCombo =
        new AssignedKeyCombo(keyComboPrefString);

    document.body.appendChild(faceGazeActionsCard);
    flush();

    assertTrue(isGestureToMacroPrefSet(
        new FaceGazeCommandPair(expectedMacro, expectedGesture)));
    assertTrue(isGestureToMacroPrefSet(keyComboCommandPair));
    assertTrue(isGestureToKeyComboPrefSet(keyComboCommandPair));

    const commandPairs = faceGazeActionsCard.get(
        FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME);
    assertEquals(2, commandPairs.length);
  });

  test('actions update prefs with added command pair', async () => {
    await initPage();

    const expectedCommandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK);
    assertFalse(isGestureToMacroPrefSet(expectedCommandPair));
    await openAddDialogAndFireCommandPairAddedEvent(expectedCommandPair);
    assertTrue(isGestureToMacroPrefSet(expectedCommandPair));
  });

  test(
      'actions update prefs with added command pair with custom keyboard shortcut',
      async () => {
        await initPage();

        const keyComboCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.JAW_OPEN);
        const keyCombo: KeyCombination = {
          key: 67,
          keyDisplay: 'c',
          modifiers: {
            ctrl: true,
          },
        };
        keyComboCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify(keyCombo));

        assertFalse(isGestureToMacroPrefSet(keyComboCommandPair));
        assertFalse(isGestureToKeyComboPrefSet(keyComboCommandPair));
        await openAddDialogAndFireCommandPairAddedEvent(keyComboCommandPair);
        assertTrue(isGestureToMacroPrefSet(keyComboCommandPair));
        assertTrue(isGestureToKeyComboPrefSet(keyComboCommandPair));
      });

  test('actions update UI to un-assign command pair gesture', async () => {
    await initPage();

    const commandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK);
    await openAddDialogAndFireCommandPairAddedEvent(commandPair);
    assertTrue(isGestureToMacroPrefSet(commandPair));
    assertActionSettingsRow(commandPair);

    const unchangedCommandPair = new FaceGazeCommandPair(
        MacroName.TOGGLE_DICTATION, FacialGesture.MOUTH_PUCKER);
    await openAddDialogAndFireCommandPairAddedEvent(unchangedCommandPair);
    assertTrue(isGestureToMacroPrefSet(unchangedCommandPair));
    assertActionSettingsRow(unchangedCommandPair);

    const reassignGestureCommandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_RIGHT, FacialGesture.EYES_BLINK);
    await openAddDialogAndFireCommandPairAddedEvent(reassignGestureCommandPair);

    assertActionSettingsRow(reassignGestureCommandPair);
    assertActionSettingsRow(unchangedCommandPair);
    assertActionSettingsRow(
        new FaceGazeCommandPair(MacroName.MOUSE_CLICK_LEFT, null));
  });

  test(
      'actions update UI to un-assign command pair gesture with custom keyboard shortcut',
      async () => {
        await initPage();

        const unchangedCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.JAW_OPEN);
        unchangedCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 68,
              keyDisplay: 'd',
            }));
        await openAddDialogAndFireCommandPairAddedEvent(unchangedCommandPair);
        assertTrue(isGestureToMacroPrefSet(unchangedCommandPair));
        assertTrue(isGestureToKeyComboPrefSet(unchangedCommandPair));
        assertActionSettingsRow(unchangedCommandPair);

        const keyComboCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.EYES_BLINK);
        keyComboCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 67,
              keyDisplay: 'c',
              modifiers: {
                ctrl: true,
              },
            }));
        await openAddDialogAndFireCommandPairAddedEvent(keyComboCommandPair);
        assertTrue(isGestureToMacroPrefSet(keyComboCommandPair));
        assertTrue(isGestureToKeyComboPrefSet(keyComboCommandPair));
        assertActionSettingsRow(keyComboCommandPair);

        const reassignGestureCommandPair = new FaceGazeCommandPair(
            MacroName.MOUSE_CLICK_RIGHT, FacialGesture.EYES_BLINK);
        await openAddDialogAndFireCommandPairAddedEvent(
            reassignGestureCommandPair);
        assertTrue(isGestureToMacroPrefSet(reassignGestureCommandPair));

        assertActionSettingsRow(reassignGestureCommandPair);
        assertActionSettingsRow(unchangedCommandPair);

        const compareCommandPair =
            new FaceGazeCommandPair(MacroName.CUSTOM_KEY_COMBINATION, null);
        compareCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 67,
              keyDisplay: 'c',
              modifiers: {
                ctrl: true,
              },
            }));
        assertActionSettingsRow(compareCommandPair);
        assertFalse(isGestureToKeyComboPrefSet(keyComboCommandPair));
      });

  test(
      'actions update UI to un-assign command pair gesture with custom keyboard shortcut with new custom keyboard shortcut',
      async () => {
        await initPage();

        const keyComboCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.JAW_OPEN);
        keyComboCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 68,
              keyDisplay: 'd',
            }));
        await openAddDialogAndFireCommandPairAddedEvent(keyComboCommandPair);
        assertTrue(isGestureToMacroPrefSet(keyComboCommandPair));
        assertTrue(isGestureToKeyComboPrefSet(keyComboCommandPair));
        assertActionSettingsRow(keyComboCommandPair);

        const reassignGestureCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.JAW_OPEN);
        reassignGestureCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 67,
              keyDisplay: 'c',
              modifiers: {
                ctrl: true,
              },
            }));
        await openAddDialogAndFireCommandPairAddedEvent(
            reassignGestureCommandPair);
        assertTrue(isGestureToMacroPrefSet(reassignGestureCommandPair));
        assertTrue(isGestureToKeyComboPrefSet(reassignGestureCommandPair));
        assertActionSettingsRow(reassignGestureCommandPair);

        const compareCommandPair =
            new FaceGazeCommandPair(MacroName.CUSTOM_KEY_COMBINATION, null);
        compareCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 68,
              keyDisplay: 'd',
            }));
        assertActionSettingsRow(compareCommandPair);
        assertFalse(isGestureToKeyComboPrefSet(keyComboCommandPair));
      });

  test('actions update UI to assign command pair gesture', async () => {
    await initPage();

    const commandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK);
    await openAddDialogAndFireCommandPairAddedEvent(commandPair);
    assertTrue(isGestureToMacroPrefSet(commandPair));
    assertActionSettingsRow(commandPair);

    const reassignGestureCommandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_RIGHT, FacialGesture.EYES_BLINK);
    await openAddDialogAndFireCommandPairAddedEvent(reassignGestureCommandPair);

    assertActionSettingsRow(reassignGestureCommandPair);
    const assignRow = assertActionSettingsRow(
        new FaceGazeCommandPair(MacroName.MOUSE_CLICK_LEFT, null));

    const assignChip = assignRow.querySelector('cros-chip');
    assertTrue(!!assignChip);
    const newGestureCommandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.JAW_OPEN);
    await openDialogAndFireCommandPairAddedEvent(
        assignChip, newGestureCommandPair);
    assertTrue(isGestureToMacroPrefSet(newGestureCommandPair));
    assertActionSettingsRow(newGestureCommandPair);

    const commandPairs = faceGazeActionsCard.get('commandPairs_');
    assertEquals(2, commandPairs.length);
  });

  test('actions update prefs based on removed command pair', async () => {
    await initPage();

    const commandPair = new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK);
    await openAddDialogAndFireCommandPairAddedEvent(commandPair);
    assertTrue(isGestureToMacroPrefSet(commandPair));
    flush();

    const removeButton =
        faceGazeActionsCard.shadowRoot!.querySelector<CrIconButtonElement>(
            '.icon-clear');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    assertFalse(isGestureToMacroPrefSet(commandPair));
  });

  test(
      'actions update prefs based on removed command pair with custom keyboard shortcut',
      async () => {
        await initPage();

        const keyComboCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.JAW_OPEN);
        const keyCombo: KeyCombination = {
          key: 67,
          keyDisplay: 'c',
          modifiers: {
            ctrl: true,
          },
        };
        keyComboCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify(keyCombo));
        await openAddDialogAndFireCommandPairAddedEvent(keyComboCommandPair);
        assertTrue(isGestureToMacroPrefSet(keyComboCommandPair));
        assertTrue(isGestureToKeyComboPrefSet(keyComboCommandPair));
        flush();

        const removeButton =
            faceGazeActionsCard.shadowRoot!.querySelector<CrIconButtonElement>(
                '.icon-clear');
        assertTrue(!!removeButton);
        removeButton.click();
        await flushTasks();

        assertFalse(isGestureToMacroPrefSet(keyComboCommandPair));
        assertFalse(isGestureToKeyComboPrefSet(keyComboCommandPair));
      });

  test('actions add button opens dialog on action page', async () => {
    await initPage();

    getAddButton().click();
    await flushTasks();

    const dialog = getDialog();
    assertEquals(AddDialogPage.SELECT_ACTION, dialog.getCurrentPageForTest());
    assertNull(dialog.commandPairToConfigure);
  });

  test(
      'actions assign gesture button opens dialog on gesture page',
      async () => {
        await initPage();

        await openAddDialogAndFireCommandPairAddedEvent(
            new FaceGazeCommandPair(MacroName.MOUSE_CLICK_LEFT, null));
        flush();

        const chip = faceGazeActionsCard.shadowRoot!.querySelector('cros-chip');
        assertTrue(!!chip);
        chip.click();
        await flushTasks();

        const dialog = getDialog();
        assertEquals(AddDialogPage.SELECT_GESTURE, dialog.initialPage);
        assertTrue(!!dialog.commandPairToConfigure);
        assertTrue(!!dialog.commandPairToConfigure.action);
      });

  test(
      'actions assign gesture button with custom keyboard shortcut opens dialog on gesture page',
      async () => {
        await initPage();

        const commandPair =
            new FaceGazeCommandPair(MacroName.CUSTOM_KEY_COMBINATION, null);
        const keyCombo: KeyCombination = {
          key: 67,
          keyDisplay: 'c',
          modifiers: {
            ctrl: true,
          },
        };
        commandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify(keyCombo));

        await openAddDialogAndFireCommandPairAddedEvent(commandPair);
        flush();

        const chip = faceGazeActionsCard.shadowRoot!.querySelector('cros-chip');
        assertTrue(!!chip);
        chip.click();
        await flushTasks();

        const dialog = getDialog();
        assertEquals(AddDialogPage.SELECT_GESTURE, dialog.initialPage);
        assertTrue(!!dialog.commandPairToConfigure);
        assertTrue(!!dialog.commandPairToConfigure.action);
        assertTrue(!!dialog.commandPairToConfigure.assignedKeyCombo);
      });

  test(
      'actions gesture button opens dialog on gesture threshold page',
      async () => {
        await initPage();

        await openAddDialogAndFireCommandPairAddedEvent(new FaceGazeCommandPair(
            MacroName.MOUSE_CLICK_LEFT, FacialGesture.BROWS_DOWN));
        flush();

        const chip = faceGazeActionsCard.shadowRoot!.querySelector('cros-chip');
        assertTrue(!!chip);
        chip.click();
        await flushTasks();

        const dialog = getDialog();
        assertEquals(AddDialogPage.GESTURE_THRESHOLD, dialog.initialPage);
        assertTrue(!!dialog.commandPairToConfigure);
        assertTrue(!!dialog.commandPairToConfigure.gesture);
      });

  test('actions dialog left click gestures is updated', async () => {
    await initPage();

    await openAddDialogAndFireCommandPairAddedEvent(new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK));
    flush();

    getAddButton().click();
    await flushTasks();

    const dialog = getDialog();
    assertEquals(AddDialogPage.SELECT_ACTION, dialog.initialPage);
    assertEquals(1, dialog.leftClickGestures.length);
  });

  test('actions adds pause/resume to the top of the card', async () => {
    await initPage();

    await openAddDialogAndFireCommandPairAddedEvent(new FaceGazeCommandPair(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK));
    flush();

    const commandPairs = faceGazeActionsCard.get('commandPairs_');
    assertEquals(1, commandPairs.length);

    const toggleCommandPair = new FaceGazeCommandPair(
        MacroName.TOGGLE_FACEGAZE, FacialGesture.JAW_OPEN);
    await openAddDialogAndFireCommandPairAddedEvent(toggleCommandPair);
    flush();

    assertEquals(2, commandPairs.length);
    assertEquals(toggleCommandPair, commandPairs[0]);
  });
});
