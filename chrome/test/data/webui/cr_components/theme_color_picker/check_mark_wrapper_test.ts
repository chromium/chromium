// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/theme_color_picker/check_mark_wrapper.js';

import type {CheckMarkWrapperElement} from 'chrome://resources/cr_components/theme_color_picker/check_mark_wrapper.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('CheckMarkWrapperTest', () => {
  let checkMarkWrapperElement: CheckMarkWrapperElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    checkMarkWrapperElement =
        document.createElement('cr-theme-color-check-mark-wrapper');
    document.body.appendChild(checkMarkWrapperElement);
  });

  test('renders check mark if checked', async () => {
    checkMarkWrapperElement.checked = true;
    await microtasksFinished();
    assertTrue(isVisible(checkMarkWrapperElement.$.svg));
  });

  test('does not render check mark if not checked', async () => {
    checkMarkWrapperElement.checked = false;
    await microtasksFinished();
    assertFalse(isVisible(checkMarkWrapperElement.$.svg));
  });
});
