// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/combobox/customize_chrome_combobox.js';

import {CustomizeChromeCombobox} from 'chrome://customize-chrome-side-panel.top-chrome/combobox/customize_chrome_combobox.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ComboboxTest', () => {
  let combobox: CustomizeChromeCombobox;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    combobox = document.createElement('customize-chrome-combobox');
    document.body.appendChild(combobox);
  });

  test('ShowsAndHides', () => {
    // Add a fake option so the dropdown isn't empty.
    const fakeOption = document.createElement('div');
    fakeOption.innerText = 'Option 1';
    combobox.appendChild(fakeOption);

    assertFalse(isVisible(combobox.$.dropdown));
    combobox.$.input.click();
    assertTrue(isVisible(combobox.$.dropdown));

    combobox.$.input.dispatchEvent(new FocusEvent('focusout'));
    assertFalse(isVisible(combobox.$.dropdown));
  });
});
