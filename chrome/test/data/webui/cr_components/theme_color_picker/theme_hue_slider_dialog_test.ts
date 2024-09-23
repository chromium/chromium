// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import type {ThemeHueSliderDialogElement} from 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('CrComponentsThemeHueSliderDialogTest', () => {
  let element: ThemeHueSliderDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-theme-hue-slider-dialog');
    document.body.appendChild(element);
  });

  test('SetsUpCrSliderValues', async () => {
    assertEquals(0, element.$.slider.min);
    assertEquals(359, element.$.slider.max);

    element.selectedHue = 200;
    await microtasksFinished();
    assertEquals(200, element.$.slider.value);
  });

  test('UpdatesCrSliderUi', async () => {
    const knobStyle =
        window.getComputedStyle(element.$.slider.$.knob, '::after');
    element.$.slider.value = 200;
    element.$.slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
    await microtasksFinished();

    // window.getComputedStyle only returns color values in rgb format, so
    // rgb(0, 170, 255) is manually converted from hsl(200, 100%, 50%).
    assertEquals('rgb(0, 170, 255)', knobStyle.backgroundColor);

    element.$.slider.value = 300;
    element.$.slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
    await microtasksFinished();

    // rgb(255, 0, 255) is manually converted from hsl(300, 100%, 50%).
    assertEquals('rgb(255, 0, 255)', knobStyle.backgroundColor);
  });

  test('UpdatesSelectedHue', () => {
    element.selectedHue = 0;

    // Changing the slider itself should not update selected hue.
    element.$.slider.value = 100;
    element.$.slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
    assertEquals(0, element.selectedHue);

    // Only on pointerup should the selectedHue change.
    element.$.slider.dispatchEvent(new PointerEvent('pointerup'));
    assertEquals(100, element.selectedHue);

    // Changing the slider itself should not update selected hue.
    element.$.slider.value = 250;
    element.$.slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
    assertEquals(100, element.selectedHue);

    // Only on keyup should the selectedHue change.
    element.$.slider.dispatchEvent(new KeyboardEvent('keyup'));
    assertEquals(250, element.selectedHue);
  });

  test('DispatchesSelectedHueChangedEvent', async () => {
    const selectedHueChangedEvent =
        eventToPromise('selected-hue-changed', element);
    element.$.slider.value = 100;
    element.$.slider.dispatchEvent(new PointerEvent('pointerup'));
    const e = await selectedHueChangedEvent;
    assertEquals(100, e.detail.selectedHue);
  });

  test('ShowsAndHides', () => {
    const anchor = document.createElement('div');
    document.body.appendChild(anchor);
    element.showAt(anchor);
    assertTrue(element.$.dialog.open);
    element.hide();
    assertFalse(element.$.dialog.open);
  });

  test('PositionsCorrectly', () => {
    const windowHeight = 1000;
    const dialogWidth = 100;
    const dialogHeight = 200;
    const anchorWidth = 50;
    const anchorHeight = 25;
    const anchorTop = 300;
    const anchorLeft = 400;

    // Force some dimensions to testing is more predictable.
    const anchor = document.createElement('div');
    anchor.style.position = 'fixed';
    anchor.style.top = `${anchorTop}px`;
    anchor.style.height = `${anchorHeight}px`;
    anchor.style.left = `${anchorLeft}px`;
    anchor.style.width = `${anchorWidth}px`;
    element.$.dialog.style.width = `${dialogWidth}px`;
    element.$.dialog.style.height = `${dialogHeight}px`;
    window.innerHeight = windowHeight;

    document.body.appendChild(anchor);
    element.showAt(anchor);

    assertEquals(`${anchorTop + anchorHeight}px`, element.$.dialog.style.top);
    assertEquals(
        `${anchorLeft + anchorWidth - dialogWidth}px`,
        element.$.dialog.style.left);
    element.hide();

    // Test that the top position changes if anchor is near bottom of window.
    const newAnchorTop = windowHeight;
    anchor.style.top = `${newAnchorTop}px`;
    element.showAt(anchor);
    assertEquals(
        `${newAnchorTop - dialogHeight}px`, element.$.dialog.style.top);
  });

  test('HidesWhenClickingOutsideDialog', () => {
    const anchor = document.createElement('div');
    document.body.appendChild(anchor);
    element.showAt(anchor);

    // Clicks within dialog should do nothing.
    element.$.dialog.dispatchEvent(
        new PointerEvent('pointerdown', {composed: true, bubbles: true}));
    assertTrue(element.$.dialog.open);

    // Clicking anywhere outside dialog should close the dialog.
    const externalElement = document.createElement('div');
    document.body.appendChild(externalElement);
    externalElement.dispatchEvent(
        new PointerEvent('pointerdown', {composed: true, bubbles: true}));
    assertFalse(element.$.dialog.open);
  });
});
