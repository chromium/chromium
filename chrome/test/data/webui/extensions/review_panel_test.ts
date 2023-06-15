// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-review-panel. */
import 'chrome://extensions/extensions.js';

import {ExtensionsReviewPanelElement, PluralStringProxyImpl} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createExtensionInfo} from './test_util.js';

suite('ExtensionsReviewPanel', function() {
  let element: ExtensionsReviewPanelElement;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('extensions-review-panel');
    const extensionItems = [
      createExtensionInfo({
        name: 'Alpha',
        id: 'a'.repeat(32),
        blacklistText: 'This extension contains malware.',
      }),
      createExtensionInfo({name: 'Bravo', id: 'b'.repeat(32)}),
      createExtensionInfo({name: 'Charlie', id: 'c'.repeat(29) + 'wxy'}),
    ];
    element.extensions = extensionItems;
    document.body.appendChild(element);
    return flushTasks();
  });

  test('ReviewPanelTextExists', async function() {
    // Review panel should be visible.
    const reviewPanelContainer = element.$.reviewPanelContainer;
    assertTrue(!!reviewPanelContainer);
    assertTrue(isVisible(reviewPanelContainer));

    // The expand button should be visible along with the review panel.
    const expandButton = element.$.expandButton;
    assertTrue(!!expandButton);
    assertTrue(isVisible(expandButton));

    // Verify that review panel heading exists.
    const headingContainer = element.$.headingText;
    assertTrue(!!headingContainer);

    // TODO(http://crbug.com/1432194): Update the unsafe extensions number
    const headingArgs = pluralString.getArgs('getPluralString')[0];
    assertEquals('safetyCheckTitle', headingArgs.messageName);
    assertEquals(1, headingArgs.itemCount);

    const descriptionArgs = pluralString.getArgs('getPluralString')[1];
    assertEquals('safetyCheckDescription', descriptionArgs.messageName);
    assertEquals(1, descriptionArgs.itemCount);

    // Verify that Remove All button exists.
    const removeAllButton = element.$.removeAllButton;
    assertTrue(!!removeAllButton);
    assertEquals(removeAllButton.innerText, 'Remove All');
  });

  test('CollapsibleList', function() {
    const expandButton = element.$.expandButton;
    assertTrue(!!expandButton);

    const extensionsList = element.shadowRoot!.querySelector('iron-collapse');
    assertTrue(!!extensionsList);

    // Button and list start out expanded.
    assertTrue(expandButton.expanded);
    assertTrue(extensionsList.opened);

    // User collapses the list.
    expandButton.click();
    flush();

    // Button and list are collapsed.
    assertFalse(expandButton.expanded);
    assertFalse(extensionsList.opened);

    // User expands the list.
    expandButton.click();
    flush();

    // Button and list are expanded.
    assertTrue(expandButton.expanded);
    assertTrue(extensionsList.opened);
  });

  test('ReviewPanelUnsafeExtensionRowsExist', async function() {
    const extensionNameContainers =
        element.shadowRoot!.querySelectorAll('.extension-row');
    assertEquals(extensionNameContainers.length, 1);
    assertEquals(
        extensionNameContainers[0]
            ?.querySelector('.extension-representation')
            ?.textContent,
        'Alpha');
  });

  // TODO(http://crbug.com/1432194): Add tests to verify action functionalities.
});
