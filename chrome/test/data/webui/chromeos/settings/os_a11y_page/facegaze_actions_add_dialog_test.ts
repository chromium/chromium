// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AddDialogPage, FaceGazeAddActionDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrSettingsPrefs, CrSliderElement, FaceGazeSubpageBrowserProxyImpl, IronListElement, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestFaceGazeSubpageBrowserProxy} from './test_facegaze_subpage_browser_proxy.js';

suite('<facegaze-actions-add-dialog>', () => {
  let faceGazeAddActionDialog: FaceGazeAddActionDialogElement;
  let browserProxy: TestFaceGazeSubpageBrowserProxy;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeAddActionDialog =
        document.createElement('facegaze-actions-add-dialog');
    faceGazeAddActionDialog.prefs = prefElement.prefs;
    document.body.appendChild(faceGazeAddActionDialog);

    // Assume default open to SELECT_ACTION page.
    assertEquals(
        AddDialogPage.SELECT_ACTION,
        faceGazeAddActionDialog.getCurrentPageForTest());
    flush();
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

  function setActionsListSelection() {
    const actionList = assertActionsList();

    // Cast to Object to satisfy typing for IronListElement.selectedItem.
    actionList.selectedItem = MacroName.MOUSE_CLICK_LEFT as Object;
  }

  function getGesturesList(): IronListElement|null {
    const gestureList: IronListElement|null =
        faceGazeAddActionDialog.shadowRoot!.querySelector<IronListElement>(
            '#faceGazeAvailableGesturesList');
    return gestureList;
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

  function getGestureSlider(): CrSliderElement|null {
    const gestureSlider =
        faceGazeAddActionDialog.shadowRoot!.querySelector<CrSliderElement>(
            '#faceGazeGestureThresholdSlider');
    return gestureSlider;
  }

  function assertGestureSlider() {
    const gestureSlider = getGestureSlider();
    assertTrue(!!gestureSlider);
  }

  function assertNullGestureSlider() {
    const gestureSlider = getGestureSlider();
    assertNull(gestureSlider);
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

  function getActionNextButton(): CrButtonElement {
    return getButton('#faceGazeAddActionNextButton');
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
    setActionsListSelection();

    const nextButton = getActionNextButton();
    assertFalse(nextButton.disabled);

    nextButton.click();
    flush();

    assertGesturesListNoSelection();
    assertNullActionsList();
  });

  test(
      'gesture page previous button changes dialog to action page',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelection();

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
      'gesture page next button disabled if no gesture is selected',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelection();

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
        assertActionsListNoSelection();
        setActionsListSelection();

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

        assertGestureSlider();
        assertNullGesturesList();
      });

  test(
      'threshold page previous button changes dialog to gesture page',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelection();

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

        assertGestureSlider();
        assertNullGesturesList();

        const previousButton = getThresholdPreviousButton();
        assertFalse(previousButton.disabled);

        previousButton.click();
        flush();

        assertGesturesList();
        assertNullGestureSlider();
      });

  test(
      'threshold page slider changes gesture confidence pref on save',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelection();

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

        assertGestureSlider();
        assertNullGesturesList();

        const gestureSlider = getGestureSlider();
        assertTrue(!!gestureSlider);

        pressAndReleaseKeyOn(gestureSlider, 39 /* right */, [], 'ArrowRight');
        flush();

        const saveButton = getSaveButton();
        saveButton.click();
        flush();

        assertTrue(isThresholdValueSetInPref(65));
      });

  test(
      'threshold page slider button changes gesture confidence pref on save',
      async () => {
        await initPage();
        assertActionsListNoSelection();
        setActionsListSelection();

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

        assertTrue(isThresholdValueSetInPref(55));
      });

  test(
      'action page switches to gesture page when initialPage value is set',
      async () => {
        await initPage();

        faceGazeAddActionDialog.actionToAssignGesture =
            MacroName.MOUSE_CLICK_LEFT;
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

        faceGazeAddActionDialog.gestureToConfigure = FacialGesture.BROWS_DOWN;
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
        setActionsListSelection();

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
});
