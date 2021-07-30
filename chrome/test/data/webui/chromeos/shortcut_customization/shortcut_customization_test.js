// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getShortcutProvider, setShortcutProviderForTesting} from 'chrome://shortcut-customization/mojo_interface_provider.js';
import {ShortcutCustomizationAppElement} from 'chrome://shortcut-customization/shortcut_customization_app.js';
import {ShortcutProviderInterface} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

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

  test('DialogOpensOnEvent', async () => {
    await flushTasks();
    // The edit dialog should not be stamped and visible.
    let editDialog = page.shadowRoot.querySelector('#editDialog');
    assertFalse(!!editDialog);

    const nav = page.shadowRoot.querySelector('navigation-view-panel');

    // Simulate the trigger event to display the dialog.
    nav.dispatchEvent(new CustomEvent('show-edit-dialog', {
      bubbles: true,
      composed: true,
      detail: /**@type {!Object}*/ (
          {description: 'test', accelerators: [{modifiers: 1 << 1, key: 'c'}]})
    }));
    await flushTasks();

    // Requery dialog.
    editDialog = page.shadowRoot.querySelector('#editDialog');
    assertTrue(!!editDialog);

    // Close the dialog.
    const dialog = editDialog.shadowRoot.querySelector('#editDialog');
    dialog.close();
    await flushTasks();

    assertFalse(dialog.open);
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
