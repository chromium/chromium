// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/shortcut_customization_app.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

export function shortcutCustomizationAppTest() {
  /** @type {?ShortcutCustomizationAppElement} */
  let page = null;

  setup(() => {
    page = /** @type {!ShortcutCustomizationAppElement} */ (
        document.createElement('shortcut-customization-app'));
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // TODO(jimmyxgong): Remove this stub test once the page has more
    // capabilities to test.
    assertTrue(!!page.shadowRoot.querySelector('navigation-view-panel'));
  });
}
