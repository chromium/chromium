// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {CheckMarkWrapperElement} from 'chrome://customize-chrome-side-panel.top-chrome/check_mark_wrapper.js';

import {assertNotStyle, assertStyle} from './test_support.js';

suite('CheckMarkWrapperTest', () => {
  let checkMarkWrapperElement: CheckMarkWrapperElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    checkMarkWrapperElement = new CheckMarkWrapperElement();
    document.body.appendChild(checkMarkWrapperElement);
  });

  test('renders check mark if checked', () => {
    checkMarkWrapperElement.checked = true;
    assertNotStyle(checkMarkWrapperElement.$.svg, 'display', 'none');
  });

  test('does not render check mark if not checked', () => {
    checkMarkWrapperElement.checked = false;
    assertStyle(checkMarkWrapperElement.$.svg, 'display', 'none');
  });
});
