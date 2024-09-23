// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/theme_color_picker/theme_color.js';

import type {ThemeColorElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {hasStyle, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';


suite('CrComponentsThemeColorTest', () => {
  let colorElement: ThemeColorElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    colorElement = document.createElement('cr-theme-color');
    document.body.appendChild(colorElement);
  });

  test('renders colors', async () => {
    colorElement.backgroundColor = {value: 0xffff0000};
    colorElement.foregroundColor = {value: 0xff00ff00};
    await microtasksFinished();

    assertTrue(hasStyle(colorElement.$.background, 'fill', 'rgb(255, 0, 0)'));
    assertTrue(hasStyle(colorElement.$.foreground, 'fill', 'rgb(0, 255, 0)'));
  });

  test('color can be checked', async () => {
    colorElement.checked = true;
    colorElement.style.width = '66px';
    colorElement.style.height = '66px';
    await microtasksFinished();

    const wrapper = colorElement.shadowRoot!.querySelector(
        'cr-theme-color-check-mark-wrapper')!;
    assertTrue(wrapper.checked);
  });

  test('color can be unchecked', async () => {
    colorElement.checked = false;
    colorElement.style.width = '66px';
    colorElement.style.height = '66px';
    await microtasksFinished();

    const wrapper = colorElement.shadowRoot!.querySelector(
        'cr-theme-color-check-mark-wrapper')!;
    assertFalse(wrapper.checked);
  });

  test('background color can be hidden', async () => {
    colorElement.backgroundColorHidden = true;
    await microtasksFinished();

    assertFalse(isVisible(colorElement.$.background));
  });

  test('background color can be shown', async () => {
    colorElement.backgroundColorHidden = false;
    await microtasksFinished();

    assertTrue(isVisible(colorElement.$.background));
  });
});
