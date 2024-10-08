// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AddDialogPage, AssignedKeyCombo, FaceGazeAddActionDialogElement, FaceGazeCommandPair, setShortcutInputProviderForTesting} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrSettingsPrefs, CrSliderElement, FaceGazeSubpageBrowserProxyImpl, IronListElement, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {VKey} from 'chrome://resources/ash/common/shortcut_input_ui/accelerator_keys.mojom-webui.js';
import {FakeShortcutInputProvider} from 'chrome://resources/ash/common/shortcut_input_ui/fake_shortcut_input_provider.js';
import {ShortcutInputElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import {Modifier} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestFaceGazeSubpageBrowserProxy} from './test_facegaze_subpage_browser_proxy.js';

declare global {
  interface HTMLElementEventMap {
    'facegaze-command-pair-added': CustomEvent<FaceGazeCommandPair>;
  }
}

suite('<facegaze-actions-add-dialog>', () => {
  let faceGazeAddActionDialog: FaceGazeAddActionDialogElement;
  let browserProxy: TestFaceGazeSubpageBrowserProxy;
  let prefElement: SettingsPrefsElement;
  let eventDetail: FaceGazeCommandPair;
  const shortcutInputProvider: FakeShortcutInputProvider =
      new FakeShortcutInputProvider();

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeAddActionDialog =
        document.createElement('facegaze-actions-add-dialog');
    faceGazeAddActionDialog.prefs = prefElement.prefs;
    setShortcutInputProviderForTesting(shortcutInputProvider);
    document.body.appendChild(faceGazeAddActionDialog);

    // Assume default open to SELECT_ACTION page.
    assertEquals(
        AddDialogPage.SELECT_ACTION,
        faceGazeAddActionDialog.getCurrentPageForTest());
    flush();

    faceGazeAddActionDialog.addEventListener(
        'facegaze-command-pair-added', onCommandPairAdded);
  }

  function onCommandPairAdded(e: CustomEvent<FaceGazeCommandPair>): void {
    eventDetail = e.detail;
  }

  function assertEventContainsCommandPair(expected: FaceGazeCommandPair): void {
    assertTrue(!!eventDetail);
    assertEquals(eventDetail.action, expected.action);
    assertEquals(eventDetail.gesture, expected.gesture);

    if (!expected.assignedKeyCombo) {
      return;
    }

    assertTrue(!!eventDetail.assignedKeyCombo);
    assertEquals(
        eventDetail.assignedKeyCombo.prefString,
        expected.assignedKeyCombo.prefString);
  }

  function getActionsList(): IronListElement|null {
    return faceGazeAddActionDialog.shadowRoot!.querySelector<IronListElement>(
        '#faceGazeAvailableActionsList');
  }

  function assertActionsList(): IronListElement {
    const actionList = getActionsList();
    assertTrue(!!actionList);
    return actionList;
  }

  function assertActionsListNoSelection(): void {
    const actionList = getActionsList();
    assertTrue(!!actionList);
    assertNull(actionList.selectedItem);
  }

  function assertNullActionsList(): void {
    const actionList = getActionsList();
    assertNull(actionList);
  }

  function setActionsListSelectionToMouseClick() {
    const actionList = assertActionsList();

    // Cast to Object to satisfy typing for IronListElement.selectedItem.
    actionList.selectedItem = MacroName.MOUSE_CLICK_LEFT as Object;
  }

  function setActionsListSelectionToCustomKeyCombo() {
    const actionList = assertActionsList();

    // Cast to Object to satisfy typing for IronListElement.selectedItem.
    actionList.selectedItem = MacroName.CUSTOM_KEY_COMBINATION as Object;
  }

  function getShortcutInput(): ShortcutInputElement|null {
    const shortcutInput =
        faceGazeAddActionDialog.shadowRoot!.querySelector<ShortcutInputElement>(
            '#shortcutInput');
    return shortcutInput;
  }

  function assertShortcutInput(): ShortcutInputElement {
    const shortcutInput = getShortcutInput();
    assertTrue(!!shortcutInput);
    return shortcutInput;
  }

  function getGesturesList(): IronListElement|null {
    const gestureList: IronListElement|null =
        faceGazeAddActionDialog.shadowRoot!.querySelector<IronListElement>(
            '#faceGazeAvailableGesturesList');
    return gestureList;
  }

  function isGestureDisplayed(gesture: FacialGesture): boolean {
    const gestureList = assertGesturesList();
    assertTrue(!!gestureList.items);
    return gestureList.items.includes(gesture);
  }

  function assertGesturesList(): IronListElement {
    const gestureList = getGesturesList();
    assertTrue(!!gestureList);
    return gestureList;
  }

  function assertGesturesListNoSelection() {
    const gestureList = assertGesturesList();
    assertNull(gestureList.selectedItem);
  }

  function assertNullGesturesList() {
    const gestureList = getGesturesList();
    assertNull(gestureList);
  }

  function setGesturesListSelection() {
    const gestureList = assertGesturesList();

    // Cast to Object to satisfy typing for IronListElement.selectedItem.
    gestureList.selectedItem = FacialGesture.BROW_INNER_UP as Object;
  }

  function assertVideoElement(): HTMLVideoElement {
    const videoElement =
        faceGazeAddActionDialog.shadowRoot!.querySelector<HTMLVideoElement>(
            '#cameraStream');
    assertTrue(!!videoElement);
    return videoElement;
  }

  function getGestureSlider(): CrSliderElement|null {
    const gestureSlider =
        faceGazeAddActionDialog.shadowRoot!.querySelector<CrSliderElement>(
            '#faceGazeGestureThresholdSlider');
    return gestureSlider;
  }

  function assertGestureSlider(): CrSliderElement {
    const gestureSlider = getGestureSlider();
    assertTrue(!!gestureSlider);
    return gestureSlider;
  }

  function assertNullGestureSlider() {
    const gestureSlider = getGestureSlider();
    assertNull(gestureSlider);
  }

  function assertGestureDynamicBar(): HTMLElement {
    const slider = assertGestureSlider();
    const sliderBar = slider.shadowRoot!.querySelector<HTMLElement>('#bar');
    assertTrue(!!sliderBar);
    return sliderBar;
  }

  function getGestureCountDiv(): HTMLElement {
    const gestureCountDiv =
        faceGazeAddActionDialog.shadowRoot!.querySelector<HTMLElement>(
            '#faceGazeGestureCount');
    assertTrue(!!gestureCountDiv);
    return gestureCountDiv;
  }

  function isThresholdValueSetInPref(value: number): boolean {
    const gesturesToConfidence = faceGazeAddActionDialog.prefs.settings.a11y
                                     .face_gaze.gestures_to_confidence.value;
    if (FacialGesture.BROW_INNER_UP in gesturesToConfidence) {
      return gesturesToConfidence[FacialGesture.BROW_INNER_UP] === value;
    }

    return false;
  }

  function getButton(id: string): CrButtonElement {
    const button =
        faceGazeAddActionDialog.shadowRoot!.querySelector<CrButtonElement>(id);
    assertTrue(!!button);
    assertTrue(isVisible(button));
    return button;
  }

  function getCancelButton(): CrButtonElement {
    return getButton('.cancel-button');
  }

  function getActionNextButton(): CrButtonElement {
    return getButton('#faceGazeAddActionNextButton');
  }

  function getCustomKeyboardNextButton(): CrButtonElement {
    return getButton('#faceGazeCustomKeyboardNextButton');
  }

  function getCustomKeyboardPreviousButton(): CrButtonElement {
    const previousButton = getButton('#faceGazeCustomKeyboardPreviousButton');
    assertFalse(previousButton.disabled);
    return previousButton;
  }

  function getGestureNextButton(): CrButtonElement {
    return getButton('#faceGazeGestureNextButton');
  }

  function getGesturePreviousButton(): CrButtonElement {
    const previousButton = getButton('#faceGazeGesturePreviousButton');
    assertFalse(previousButton.disabled);
    return previousButton;
  }

  function getThresholdPreviousButton(): CrButtonElement {
    const previousButton = getButton('#faceGazeThresholdPreviousButton');
    assertFalse(previousButton.disabled);
    return previousButton;
  }

  function getSaveButton(): CrButtonElement {
    const saveButton = getButton('#faceGazeActionDialogSaveButton');
    assertFalse(saveButton.disabled);
    return saveButton;
  }

  function navigateToThresholdPage(): void {
    assertActionsListNoSelection();
    setActionsListSelectionToMouseClick();

    const actionNextButton = getActionNextButton();
    assertFalse(actionNextButton.disabled);
    actionNextButton.click();
    flush();

    assertGesturesListNoSelection();
    setGesturesListSelection();

    const gestureNextButton = getGestureNextButton();
    assertFalse(gestureNextButton.disabled);
    gestureNextButton.click();
    flush();

    assertVideoElement();
    assertGestureSlider();
    assertGestureDynamicBar();
    assertNullGesturesList();
  }

  setup(() => {
    browserProxy = new TestFaceGazeSubpageBrowserProxy();
    FaceGazeSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
    browserProxy.reset();
  });

  test(
      'action page next button disabled if no action is selected', async () => {
        await initPage();
        assertActionsListNoSelection();

        const nextButton = getActionNextButton();
        assertTrue(nextButton.disabled);
      });

  test('action page next button changes dialog to gesture page', async () => {
    await initPage();
    assertActionsListNoSelection();
    setActionsListSelectionToMouseClick();

    const nextButton = getActionNextButton();
    assertFalse(nextButton.disabled);

    nextButton.click();
    flush();

    assertGesturesListNoSelection();
    assertNullActionsList();
  });

  test(
      'action page next button changes dialog to custom keyboard shortcut page when custom keyboard macro selected',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToCustomKeyCombo();

        const nextButton = getActionNextButton();
        assertFalse(nextButton.disabled);

        nextButton.click();
        flush();

        assertShortcutInput();
      });

  test(
      'custom keyboard page previous button changes dialog to action page',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToCustomKeyCombo();

        const nextButton = getActionNextButton();
        assertFalse(nextButton.disabled);

        nextButton.click();
        flush();

        assertShortcutInput();

        const previousButton = getCustomKeyboardPreviousButton();
        previousButton.click();
        flush();

        assertActionsListNoSelection();
      });

  test(
      'custom keyboard page next button disabled if no keyboard shortcut selected',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToCustomKeyCombo();

        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);

        actionNextButton.click();
        flush();

        assertShortcutInput();

        const keyboardNextButton = getCustomKeyboardNextButton();
        assertTrue(keyboardNextButton.disabled);
      });

  test(
      'custom keyboard page next button changes dialog to gesture page',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToCustomKeyCombo();

        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);

        actionNextButton.click();
        flush();

        assertShortcutInput();

        const keyEvent = {
          vkey: VKey.kKeyC,
          domCode: 0,
          domKey: 0,
          modifiers: Modifier.CONTROL,
          keyDisplay: 'c',
        };

        shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
        shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
        await flushTasks();

        const keyboardNextButton = getCustomKeyboardNextButton();
        assertFalse(keyboardNextButton.disabled);
        keyboardNextButton.click();
        flush();

        assertGesturesListNoSelection();
      });

  test(
      'gesture page displayed gestures excludes single gesture assigned to left click',
      async () => {
        await initPage();
        faceGazeAddActionDialog.leftClickGestures =
            [FacialGesture.BROW_INNER_UP];
        setActionsListSelectionToMouseClick();
        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);
        actionNextButton.click();
        await flushTasks();

        assertFalse(isGestureDisplayed(FacialGesture.BROW_INNER_UP));
      });

  test(
      'gesture page displayed gestures does not exclude gestures if multiple assigned to left click',
      async () => {
        await initPage();
        faceGazeAddActionDialog.leftClickGestures =
            [FacialGesture.BROW_INNER_UP, FacialGesture.JAW_OPEN];
        setActionsListSelectionToMouseClick();
        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);
        actionNextButton.click();
        await flushTasks();

        assertTrue(isGestureDisplayed(FacialGesture.BROW_INNER_UP));
        assertTrue(isGestureDisplayed(FacialGesture.JAW_OPEN));
      });

  test(
      'gesture page does not display look gestures for actions dependent on mouse location',
      async () => {
        await initPage();
        setActionsListSelectionToMouseClick();
        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);
        actionNextButton.click();
        await flushTasks();

        assertFalse(isGestureDisplayed(FacialGesture.EYES_LOOK_DOWN));
        assertFalse(isGestureDisplayed(FacialGesture.EYES_LOOK_LEFT));
        assertFalse(isGestureDisplayed(FacialGesture.EYES_LOOK_RIGHT));
        assertFalse(isGestureDisplayed(FacialGesture.EYES_LOOK_UP));
      });

  test(
      'gesture page previous button changes dialog to action page',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToMouseClick();

        const nextButton = getActionNextButton();
        assertFalse(nextButton.disabled);

        nextButton.click();
        flush();

        assertGesturesListNoSelection();

        const previousButton = getGesturePreviousButton();
        assertFalse(previousButton.disabled);

        previousButton.click();
        flush();

        assertActionsList();
        assertNullGesturesList();
      });

  test(
      'gesture page previous button changes dialog to custom keyboard shortcut page when custom keyboard macro selected',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToCustomKeyCombo();

        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);

        actionNextButton.click();
        flush();

        assertShortcutInput();

        const keyEvent = {
          vkey: VKey.kKeyC,
          domCode: 0,
          domKey: 0,
          modifiers: Modifier.CONTROL,
          keyDisplay: 'c',
        };

        shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
        shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
        await flushTasks();

        const keyboardNextButton = getCustomKeyboardNextButton();
        assertFalse(keyboardNextButton.disabled);
        keyboardNextButton.click();
        flush();

        assertGesturesListNoSelection();

        const previousButton = getGesturePreviousButton();
        assertFalse(previousButton.disabled);

        previousButton.click();
        flush();

        assertShortcutInput();
      });

  test(
      'gesture page next button disabled if no gesture is selected',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToMouseClick();

        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);

        actionNextButton.click();
        flush();

        assertGesturesListNoSelection();

        const gestureNextButton = getGestureNextButton();
        assertTrue(gestureNextButton.disabled);
      });

  test(
      'gesture page next button changes dialog to threshold page', async () => {
        await initPage();
        navigateToThresholdPage();
      });

  test(
      'threshold page previous button changes dialog to gesture page',
      async () => {
        await initPage();
        navigateToThresholdPage();

        const previousButton = getThresholdPreviousButton();
        assertFalse(previousButton.disabled);

        previousButton.click();
        flush();

        assertGesturesList();
        assertNullGestureSlider();
      });

  test('threshold page cancel button closes dialog', async () => {
    await initPage();
    navigateToThresholdPage();

    const cancelButton = getCancelButton();
    cancelButton.click();
    flush();

    assertFalse(faceGazeAddActionDialog.$.dialog.open);
  });

  test(
      'threshold page slider changes gesture confidence pref on save',
      async () => {
        await initPage();
        navigateToThresholdPage();

        const gestureSlider = getGestureSlider();
        assertTrue(!!gestureSlider);

        pressAndReleaseKeyOn(gestureSlider, 39 /* right */, [], 'ArrowRight');
        flush();

        const saveButton = getSaveButton();
        saveButton.click();
        flush();

        assertTrue(isThresholdValueSetInPref(65));
        assertFalse(faceGazeAddActionDialog.$.dialog.open);
      });

  test(
      'threshold page slider button changes gesture confidence pref and fires event with command pair on save',
      async () => {
        await initPage();
        navigateToThresholdPage();

        const gestureSlider = getGestureSlider();
        assertTrue(!!gestureSlider);

        const decrementButton =
            getButton('#faceGazeGestureThresholdDecrementButton');
        decrementButton.click();
        flush();

        const saveButton = getSaveButton();
        saveButton.click();
        flush();

        assertTrue(isThresholdValueSetInPref(55));
        assertEventContainsCommandPair(new FaceGazeCommandPair(
            MacroName.MOUSE_CLICK_LEFT, FacialGesture.BROW_INNER_UP));
        assertFalse(faceGazeAddActionDialog.$.dialog.open);
      });

  test(
      'threshold page save button fires event with command pair containing custom keyboard shortcut',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToCustomKeyCombo();

        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);
        actionNextButton.click();
        flush();

        assertShortcutInput();

        const keyEvent = {
          vkey: VKey.kKeyC,
          domCode: 0,
          domKey: 0,
          modifiers: Modifier.CONTROL,
          keyDisplay: 'c',
        };

        shortcutInputProvider.sendKeyPressEvent(keyEvent, keyEvent);
        shortcutInputProvider.sendKeyReleaseEvent(keyEvent, keyEvent);
        await flushTasks();

        const keyboardNextButton = getCustomKeyboardNextButton();
        assertFalse(keyboardNextButton.disabled);
        keyboardNextButton.click();
        flush();

        assertGesturesListNoSelection();
        setGesturesListSelection();

        const gestureNextButton = getGestureNextButton();
        assertFalse(gestureNextButton.disabled);
        gestureNextButton.click();
        flush();

        assertGestureSlider();
        assertNullGesturesList();

        const gestureSlider = getGestureSlider();
        assertTrue(!!gestureSlider);

        const decrementButton =
            getButton('#faceGazeGestureThresholdDecrementButton');
        decrementButton.click();
        flush();

        const saveButton = getSaveButton();
        saveButton.click();
        flush();

        const expectedCommandPair = new FaceGazeCommandPair(
            MacroName.CUSTOM_KEY_COMBINATION, FacialGesture.BROW_INNER_UP);
        expectedCommandPair.assignedKeyCombo =
            new AssignedKeyCombo(JSON.stringify({
              key: 67,
              keyDisplay: 'c',
              modifiers: {
                ctrl: true,
              },
            }));
        assertEventContainsCommandPair(expectedCommandPair);
        assertFalse(faceGazeAddActionDialog.$.dialog.open);
      });

  test(
      'action page switches to gesture page when initialPage value is set',
      async () => {
        await initPage();

        faceGazeAddActionDialog.commandPairToConfigure =
            new FaceGazeCommandPair(MacroName.MOUSE_CLICK_LEFT, null);
        faceGazeAddActionDialog.initialPage = AddDialogPage.SELECT_GESTURE;
        flush();

        assertGesturesListNoSelection();

        const previousButton =
            faceGazeAddActionDialog.shadowRoot!.querySelector<CrButtonElement>(
                '#faceGazeGesturePreviousButton');
        assertNull(previousButton);
      });

  test(
      'action page switches to threshold page when initialPage value is set',
      async () => {
        await initPage();

        faceGazeAddActionDialog.commandPairToConfigure =
            new FaceGazeCommandPair(
                MacroName.MOUSE_CLICK_LEFT, FacialGesture.BROWS_DOWN);
        faceGazeAddActionDialog.initialPage = AddDialogPage.GESTURE_THRESHOLD;
        flush();

        assertGestureSlider();

        // Previous button should not be shown.
        const previousButton =
            faceGazeAddActionDialog.shadowRoot!.querySelector<CrButtonElement>(
                '#faceGazeThresholdPreviousButton');
        assertNull(previousButton);
      });

  test(
      'browser proxy sends event when page changes to and from threshold page',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelectionToMouseClick();

        const actionNextButton = getActionNextButton();
        assertFalse(actionNextButton.disabled);
        actionNextButton.click();
        flush();
        assertGesturesListNoSelection();
        setGesturesListSelection();
        assertEquals(
            0, browserProxy.getCallCount('toggleGestureInfoForSettings'));

        const gestureNextButton = getGestureNextButton();
        assertFalse(gestureNextButton.disabled);
        gestureNextButton.click();
        flush();
        assertEquals(
            1, browserProxy.getCallCount('toggleGestureInfoForSettings'));
        assertTrue(browserProxy.getArgs('toggleGestureInfoForSettings')[0][0]);
        assertGestureSlider();
        assertNullGesturesList();

        const previousButton = getThresholdPreviousButton();
        assertFalse(previousButton.disabled);
        previousButton.click();
        flush();
        assertEquals(
            2, browserProxy.getCallCount('toggleGestureInfoForSettings'));
        assertFalse(browserProxy.getArgs('toggleGestureInfoForSettings')[1][0]);
      });

  test(
      'gesture detection count updates when gesture info received with selected gesture over threshold',
      async () => {
        await initPage();
        navigateToThresholdPage();

        webUIListenerCallback('settings.sendGestureInfoToSettings', [
          {gesture: FacialGesture.BROW_INNER_UP, confidence: 70},
          {gesture: FacialGesture.BROW_INNER_UP, confidence: 50},
        ]);

        const gestureCountDiv = getGestureCountDiv();

        // Default confidence threshold is 60, so only one gesture should
        // register as detected.
        assertEquals(`Detected 1 time`, gestureCountDiv.innerText);
      });

  test(
      'gesture detection count updates when gesture info received with multiple selected gestures over threshold',
      async () => {
        await initPage();
        navigateToThresholdPage();

        webUIListenerCallback('settings.sendGestureInfoToSettings', [
          {gesture: FacialGesture.BROW_INNER_UP, confidence: 70},
          {gesture: FacialGesture.BROW_INNER_UP, confidence: 80},
        ]);

        const gestureCountDiv = getGestureCountDiv();

        // Default confidence threshold is 60, so two gestures should register
        // as detected.
        assertEquals(`Detected 2 times`, gestureCountDiv.innerText);
      });

  test(
      'gesture detection count does not update when gesture info received with non-selected gesture',
      async () => {
        await initPage();
        navigateToThresholdPage();

        webUIListenerCallback('settings.sendGestureInfoToSettings', [
          {gesture: FacialGesture.JAW_LEFT, confidence: 70},
          {gesture: FacialGesture.EYES_BLINK, confidence: 50},
        ]);

        const gestureCountDiv = getGestureCountDiv();
        assertEquals(`Not detected`, gestureCountDiv.innerText);
      });
  test(
      'gesture threshold dynamic bar updates when gesture info received with selected gesture info at any confidence',
      async () => {
        await initPage();
        navigateToThresholdPage();

        const sliderBar = assertGestureDynamicBar();

        webUIListenerCallback('settings.sendGestureInfoToSettings', [
          {gesture: FacialGesture.BROW_INNER_UP, confidence: 70},
        ]);
        assertEquals('70%', sliderBar.style.width);

        webUIListenerCallback('settings.sendGestureInfoToSettings', [
          {gesture: FacialGesture.BROW_INNER_UP, confidence: 30},
        ]);
        assertEquals('30%', sliderBar.style.width);
      });
});
