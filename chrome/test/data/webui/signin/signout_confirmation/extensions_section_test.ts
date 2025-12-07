// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signout-confirmation/signout_confirmation.js';

import type {ExtensionsSectionElement} from 'chrome://signout-confirmation/signout_confirmation.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_ACCOUNT_EXTENSIONS = [
  {
    name: 'extension 1',
    iconUrl: 'icon.png',
  },
  {
    name: 'extension 2',
    iconUrl: 'icon_2.png',
  },
];

suite('SignoutConfirmationViewTest', function() {
  let extensionsSection: ExtensionsSectionElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    extensionsSection = document.createElement('extensions-section');
    extensionsSection.accountExtensions = TEST_ACCOUNT_EXTENSIONS;

    document.body.append(extensionsSection);
  });

  test('HeaderElements', function() {
    assertTrue(isVisible(extensionsSection));

    // All header elements should be visible by default.
    assertTrue(isChildVisible(extensionsSection, '#checkbox'));
    assertTrue(isChildVisible(extensionsSection, '#title-container'));
    assertTrue(isChildVisible(extensionsSection, '#tooltip-icon'));
    assertTrue(isChildVisible(extensionsSection, '#expandButton'));
  });

  test('AccountExtensionsList', async function() {
    assertTrue(isVisible(extensionsSection));

    const expandButton = extensionsSection.$.expandButton;
    const collapsePart = extensionsSection.$.collapse;
    assertTrue(isVisible(expandButton));

    // Account extensions should be collapsed by default.
    assertFalse(collapsePart.opened);

    // Expand the collapse section.
    expandButton.click();
    await microtasksFinished();
    assertTrue(collapsePart.opened);

    // Test that the number of account extensions shown match.
    const accountExtensions =
        extensionsSection.shadowRoot.querySelectorAll<HTMLElement>(
            '.account-extension');
    assertEquals(TEST_ACCOUNT_EXTENSIONS.length, accountExtensions.length);

    // Collapse it again.
    expandButton.click();
    await microtasksFinished();
    assertFalse(collapsePart.opened);
  });
});
