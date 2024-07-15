// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {FaceGazeAddActionDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrSliderElement, IronListElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<facegaze-actions-add-dialog>', () => {
  let faceGazeAddActionDialog: FaceGazeAddActionDialogElement;

  async function initPage() {
    faceGazeAddActionDialog =
        document.createElement('facegaze-actions-add-dialog');
    document.body.appendChild(faceGazeAddActionDialog);
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
    actionList.selectedItem = {
      action: MacroName.MOUSE_CLICK_LEFT,
      displayText: 'Click a mouse button',
    };
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

    gestureList.selectedItem = {
      value: FacialGesture.BROW_INNER_UP,
      displayText: 'Brow inner up',
    };
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

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
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

        const previousButton = getThresholdPreviousButton();
        assertFalse(previousButton.disabled);

        previousButton.click();
        flush();

        assertGesturesList();
        assertNullGestureSlider();
      });
});
