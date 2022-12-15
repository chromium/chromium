// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertNotStyle, assertStyle} from './test_support.js';

suite('ColorTest', () => {
  let colorElement: ColorElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    colorElement = new ColorElement();
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

    const wrapper = colorElement.shadowRoot!.querySelector(
        'customize-chrome-check-mark-wrapper')!;
    assertTrue(wrapper.checked);
    const svg = colorElement.shadowRoot!.querySelector('svg')!;
    assertStyle(svg, 'width', '46px');
    assertStyle(svg, 'height', '46px');
    const background = colorElement.shadowRoot!.querySelector('#background')!;
    assertStyle(background, 'r', '25px');
  });

  test('color can be unchecked', () => {
    colorElement.checked = false;

    const wrapper = colorElement.shadowRoot!.querySelector(
        'customize-chrome-check-mark-wrapper')!;
    assertFalse(wrapper.checked);
    const svg = colorElement.shadowRoot!.querySelector('svg')!;
    assertStyle(svg, 'width', '50px');
    assertStyle(svg, 'height', '50px');
    const background = colorElement.shadowRoot!.querySelector('#background')!;
    assertStyle(background, 'r', '24px');
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
