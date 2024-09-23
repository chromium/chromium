// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CheckMarkWrapperElement} from 'chrome://customize-chrome-side-panel.top-chrome/check_mark_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle} from './test_support.js';

suite('CheckMarkWrapperTest', () => {
  let checkMarkWrapperElement: CheckMarkWrapperElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    checkMarkWrapperElement = new CheckMarkWrapperElement();
    document.body.appendChild(checkMarkWrapperElement);
  });

  test('renders check mark if checked', async () => {
    checkMarkWrapperElement.checked = true;
    await microtasksFinished();
    assertNotStyle(checkMarkWrapperElement.$.circle, 'display', 'none');
  });

  test('does not render check mark if not checked', async () => {
    checkMarkWrapperElement.checked = false;
    await microtasksFinished();
    assertStyle(checkMarkWrapperElement.$.circle, 'display', 'none');
  });
});
