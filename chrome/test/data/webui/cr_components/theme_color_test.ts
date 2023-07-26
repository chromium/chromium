// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ThemeColorElement} from 'chrome://resources/cr_components/theme_color_picker/theme_color.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertNotStyle, assertStyle} from './test_support.js';

suite('CrComponentsThemeColorTest', () => {
  let colorElement: ThemeColorElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    colorElement = new ThemeColorElement();
    document.body.appendChild(colorElement);
  });

  test('renders colors', () => {
    colorElement.backgroundColor = {value: 0xffff0000};
    colorElement.foregroundColor = {value: 0xff00ff00};

    assertStyle(colorElement.$.background, 'fill', 'rgb(255, 0, 0)');
    assertStyle(colorElement.$.foreground, 'fill', 'rgb(0, 255, 0)');
  });

  test('color can be checked', () => {
    colorElement.checked = true;
    colorElement.style.width = '66px';
    colorElement.style.height = '66px';

    const wrapper = colorElement.shadowRoot!.querySelector(
        'cr-theme-color-check-mark-wrapper')!;
    assertTrue(wrapper.checked);
    const svg = colorElement.shadowRoot!.querySelector('svg')!;
    assertStyle(svg, 'width', '50px');
    assertStyle(svg, 'height', '50px');
    assertStyle(svg, 'border', '0px none rgb(0, 0, 0)');
  });

  test('color can be unchecked', () => {
    colorElement.checked = false;
    colorElement.style.width = '66px';
    colorElement.style.height = '66px';

    const wrapper = colorElement.shadowRoot!.querySelector(
        'cr-theme-color-check-mark-wrapper')!;
    assertFalse(wrapper.checked);
    const svg = colorElement.shadowRoot!.querySelector('svg')!;
    assertStyle(svg, 'width', '50px');
    assertStyle(svg, 'height', '50px');
    assertStyle(svg, 'border', '1px solid rgba(0, 0, 0, 0)');
  });

  test('background color can be hidden', () => {
    colorElement.backgroundColorHidden = true;

    assertStyle(colorElement.$.background, 'display', 'none');
  });

  test('background color can be shown', () => {
    colorElement.backgroundColorHidden = false;

    assertNotStyle(colorElement.$.background, 'display', 'none');
  });
});
