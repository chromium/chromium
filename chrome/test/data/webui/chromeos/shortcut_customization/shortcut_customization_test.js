// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getShortcutProvider, setShortcutProviderForTesting} from 'chrome://shortcut-customization/mojo_interface_provider.js';
import {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/shortcut_customization_app.js';
import {ShortcutProviderInterface} from 'chrome://shortcut-customization/shortcut_types.js';

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

  suite('FakeMojoProviderTest', () => {
    test('SettingGettingTestProvider', () => {
      // TODO(zentaro): Replace with fake when built.
      let fake_provider =
          /** @type {!ShortcutProviderInterface} */ (new Object());
      setShortcutProviderForTesting(fake_provider);
      assertEquals(fake_provider, getShortcutProvider());
    });
  });
}
