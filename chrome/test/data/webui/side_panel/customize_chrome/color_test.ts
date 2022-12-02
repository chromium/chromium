// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorElement} from 'chrome://customize-chrome-side-panel.top-chrome/color.js';

import {assertStyle} from './test_support.js';

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
});
