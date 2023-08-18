// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import {ThemeHueSliderDialogElement} from 'chrome://resources/cr_components/theme_color_picker/theme_hue_slider_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('CrComponentsThemeHueSliderDialogTest', () => {
  let element: ThemeHueSliderDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-theme-hue-slider-dialog');
    document.body.appendChild(element);
  });

  test('SetsUpCrSliderValues', () => {
    assertEquals(0, element.$.slider.min);
    assertEquals(359, element.$.slider.max);

    element.selectedHue = 200;
    assertEquals(200, element.$.slider.value);
  });

  test('UpdatesCrSliderUi', () => {
    const knobStyle = window.getComputedStyle(element.$.slider.$.knob);
    element.$.slider.value = 200;
    element.$.slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));

    // window.getComputedStyle only returns color values in rgb format, so
    // rgb(0, 170, 255) is manually converted from hsl(200, 100%, 50%).
    assertEquals('rgb(0, 170, 255)', knobStyle.backgroundColor);

    element.$.slider.value = 300;
    element.$.slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
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
    assertTrue(element.$.crActionMenu.getDialog().open);
    element.hide();
    assertFalse(element.$.crActionMenu.getDialog().open);
  });
});
