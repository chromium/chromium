// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AddDialogPage, FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME, FaceGazeActionsCardElement, FaceGazeAddActionDialogElement, FaceGazeCommandPair} from 'chrome://os-settings/lazy_load.js';
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

  let faceGazeActionsCard: FaceGazeActionsCardElement;
  let prefElement: SettingsPrefsElement;

  function isCommandPairSetInPrefs(
      expectedMacro: MacroName, expectedGesture: FacialGesture): boolean {
    const assignedGestures = {...faceGazeActionsCard.prefs.settings.a11y
                                  .face_gaze.gestures_to_macros.value};
    for (const [currentGesture, assignedMacro] of Object.entries(
             assignedGestures)) {
      if (expectedGesture === currentGesture &&
          expectedMacro === assignedMacro) {
        return true;
      }
    }

    return false;
  }

  async function fireCommandPairAddedEvent(
      macro: MacroName, gesture: FacialGesture|null) {
    getAddButton().click();
    await flushTasks();

    const dialog = getDialog();

    const commandPair = new FaceGazeCommandPair(macro, gesture);
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

    document.body.appendChild(faceGazeActionsCard);
    flush();

    assertTrue(isCommandPairSetInPrefs(expectedMacro, expectedGesture));

    const commandPairs = faceGazeActionsCard.get(
        FaceGazeActionsCardElement.FACEGAZE_COMMAND_PAIRS_PROPERTY_NAME);
    assertEquals(1, commandPairs.length);
  });

  test('actions update prefs with added command pair', async () => {
    await initPage();

    const expectedMacro: MacroName = MacroName.MOUSE_CLICK_LEFT;
    const expectedGesture: FacialGesture = FacialGesture.EYES_BLINK;
    assertFalse(isCommandPairSetInPrefs(expectedMacro, expectedGesture));
    await fireCommandPairAddedEvent(expectedMacro, expectedGesture);
    assertTrue(isCommandPairSetInPrefs(expectedMacro, expectedGesture));
  });

  test('actions update prefs based on removed command pair', async () => {
    await initPage();

    const expectedMacro: MacroName = MacroName.MOUSE_CLICK_LEFT;
    const expectedGesture: FacialGesture = FacialGesture.EYES_BLINK;
    await fireCommandPairAddedEvent(expectedMacro, expectedGesture);
    assertTrue(isCommandPairSetInPrefs(expectedMacro, expectedGesture));
    flush();

    const removeButton =
        faceGazeActionsCard.shadowRoot!.querySelector<CrIconButtonElement>(
            '.icon-clear');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    assertFalse(isCommandPairSetInPrefs(expectedMacro, expectedGesture));
  });

  test('actions add button opens dialog on action page', async () => {
    await initPage();

    getAddButton().click();
    await flushTasks();

    const dialog = getDialog();
    assertEquals(AddDialogPage.SELECT_ACTION, dialog.getCurrentPageForTest());
    assertNull(dialog.actionToAssignGesture);
  });

  test(
      'actions assign gesture button opens dialog on gesture page',
      async () => {
        await initPage();

        await fireCommandPairAddedEvent(MacroName.MOUSE_CLICK_LEFT, null);
        flush();

        const chip = faceGazeActionsCard.shadowRoot!.querySelector('cros-chip');
        assertTrue(!!chip);
        chip.click();
        await flushTasks();

        const dialog = getDialog();
        assertEquals(AddDialogPage.SELECT_GESTURE, dialog.initialPage);
        assertTrue(!!dialog.actionToAssignGesture);
      });

  test('actions gesture button opens dialog on gesture page', async () => {
    await initPage();

    await fireCommandPairAddedEvent(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.BROWS_DOWN);
    flush();

    const chip = faceGazeActionsCard.shadowRoot!.querySelector('cros-chip');
    assertTrue(!!chip);
    chip.click();
    await flushTasks();

    const dialog = getDialog();
    assertEquals(AddDialogPage.GESTURE_THRESHOLD, dialog.initialPage);
    assertTrue(!!dialog.gestureToConfigure);
  });

  test('actions dialog left click gestures is updated', async () => {
    await initPage();

    await fireCommandPairAddedEvent(
        MacroName.MOUSE_CLICK_LEFT, FacialGesture.EYES_BLINK);
    flush();

    getAddButton().click();
    await flushTasks();

    const dialog = getDialog();
    assertEquals(AddDialogPage.SELECT_ACTION, dialog.initialPage);
    assertEquals(1, dialog.leftClickGestures.length);
  });
});
