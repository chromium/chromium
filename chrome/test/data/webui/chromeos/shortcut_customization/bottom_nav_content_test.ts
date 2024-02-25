// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/bottom_nav_content.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ShortcutsBottomNavContentElement} from 'chrome://shortcut-customization/js/bottom_nav_content.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

export function initBottomNavContentElement():
    ShortcutsBottomNavContentElement {
  const element = document.createElement('shortcuts-bottom-nav-content');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('BottomNavContentTest', function() {
  let contentElement: ShortcutsBottomNavContentElement|null = null;

  teardown(() => {
    if (contentElement) {
      contentElement.remove();
    }
    contentElement = null;
  });

  test('KeyboardSettingsLink', async () => {
    contentElement = initBottomNavContentElement();
    const linkElement = strictQuery(
        'a#keyboardSettingsLink', contentElement.shadowRoot, HTMLAnchorElement);
    assertTrue(!!linkElement);
  });

  test('RestoreAllButtonNotHidden', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    contentElement = initBottomNavContentElement();
    contentElement.restoreAllButtonHidden = false;
    const restoreAllButton =
        contentElement.shadowRoot!.querySelector<CrButtonElement>(
            'cr-button#restoreAllButton');
    assertFalse(!!restoreAllButton?.hidden);
  });

  test('RestoreAllButtonHidden', async () => {
    loadTimeData.overrideValues({isCustomizationAllowed: true});
    contentElement = initBottomNavContentElement();
    contentElement.restoreAllButtonHidden = true;
    const restoreAllButton =
        contentElement.shadowRoot!.querySelector<CrButtonElement>(
            'cr-button#restoreAllButton');
    assertTrue(!!restoreAllButton?.hidden);
  });

  test('RestoreAllButtonShownWhenCustomizationDisabled', async () => {
    // Even if customization is disabled, if the property
    // `restoreAllButtonHidden` is false, the button will be shown. It's up to
    // the parent element to check if customization is enabled.
    loadTimeData.overrideValues({isCustomizationAllowed: false});
    contentElement = initBottomNavContentElement();
    contentElement.restoreAllButtonHidden = false;
    const restoreAllButton =
        contentElement.shadowRoot!.querySelector<CrButtonElement>(
            'cr-button#restoreAllButton');
    assertFalse(!!restoreAllButton?.hidden);
  });
});
