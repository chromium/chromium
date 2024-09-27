// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-review-panel. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsReviewPanelElement} from 'chrome://extensions/extensions.js';
import {PluralStringProxyImpl} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createExtensionInfo, MockItemDelegate} from './test_util.js';

suite('ExtensionsReviewPanel', function() {
  let element: ExtensionsReviewPanelElement;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
    loadTimeData.overrideValues({'safetyHubShowReviewPanel': true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('extensions-review-panel');
    const extensionItems = [
      createExtensionInfo({
        name: 'Alpha',
        id: 'a'.repeat(32),
        safetyCheckText: {panelString: 'This extension contains malware.'},
      }),
    ];
    element.extensions = extensionItems;
    document.body.appendChild(element);
    return microtasksFinished();
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

    const safetyHubHeader = element.$.safetyHubTitleContainer;
    assertTrue(isVisible(safetyHubHeader));
  });

  test('CollapsibleList', async function() {
    const expandButton = element.$.expandButton;
    assertTrue(!!expandButton);

    const extensionsList = element.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!extensionsList);

    // Button and list start out expanded.
    assertTrue(expandButton.expanded);
    assertTrue(extensionsList.opened);

    // User collapses the list.
    expandButton.click();
    await microtasksFinished();

    // Button and list are collapsed.
    assertFalse(expandButton.expanded);
    assertFalse(extensionsList.opened);

    // User expands the list.
    expandButton.click();
    await microtasksFinished();

    // Button and list are expanded.
    assertTrue(expandButton.expanded);
    assertTrue(extensionsList.opened);
  });

  test('ReviewPanelUnsafeExtensionRowsExist', async function() {
    const extensionNameContainers =
        element.shadowRoot!.querySelectorAll('.panel-extension-row');
    assertEquals(extensionNameContainers.length, 1);
    assertEquals(
        extensionNameContainers[0]
            ?.querySelector('.extension-representation')
            ?.textContent,
        'Alpha');
  });

  test(
      'CompletionStateShouldNotBeShownIfNoExtensionsAndNoAction',
      async function() {
        const completionTextContainer =
            element.shadowRoot!.querySelector('.completion-container');
        assertTrue(!!completionTextContainer);
        assertFalse(isVisible(completionTextContainer));

        element.extensions = [];
        await microtasksFinished();

        assertFalse(isVisible(completionTextContainer));
      });

  test('CompletionStateShouldBeShownAfterDeletingItems', async function() {
    const completionTextContainer =
        element.shadowRoot!.querySelector('.completion-container');
    assertFalse(isVisible(completionTextContainer));
    class MockUninstallItemDelegate extends MockItemDelegate {
      override uninstallItem(id: string): Promise<void> {
        // Mock deleting the extension.
        element.extensions =
            element.extensions.filter(extension => extension.id !== id);
        return Promise.resolve();
      }
      override setItemSafetyCheckWarningAcknowledged(): void {}
    }
    element.delegate = new MockUninstallItemDelegate();
    element.shadowRoot!.querySelector('cr-icon-button')?.click();
    await microtasksFinished();
    const completionText = pluralString.getArgs('getPluralString')[5];
    assertTrue(!!completionTextContainer);
    assertTrue(isVisible(completionTextContainer));
    assertEquals(completionText.messageName, 'safetyCheckAllDoneForNow');
    assertEquals(completionText.itemCount, 1);
  });

  test(
      'CompletionStateShouldBeShownAfterDeletingMultipleExtensions',
      async function() {
        const completionTextContainer =
            element.shadowRoot!.querySelector('.completion-container');
        assertFalse(isVisible(completionTextContainer));
        class MockDeleteItemDelegate extends MockItemDelegate {
          override deleteItems(ids: string[]) {
            element.extensions = element.extensions.filter(
                extension => !ids.includes(extension.id));
            return Promise.resolve();
          }
          override setItemSafetyCheckWarningAcknowledged(): void {}
        }
        const extensionItems = [
          createExtensionInfo({
            name: 'Alpha',
            id: 'a'.repeat(32),
            safetyCheckText: {panelString: 'This extension contains malware.'},
          }),
          createExtensionInfo({
            name: 'Bravo',
            id: 'b'.repeat(32),
            safetyCheckText: {panelString: 'This extension contains malware.'},
          }),
          createExtensionInfo({
            name: 'Charlie',
            id: 'c'.repeat(29),
            safetyCheckText: {panelString: 'This extension contains malware.'},
          }),
        ];
        element.extensions = extensionItems;
        element.delegate = new MockDeleteItemDelegate();

        // Wait for the UI to finish rendering and reset plural string calls.
        await microtasksFinished();
        pluralString.resetResolver('getPluralString');

        // Wait until the async response comes back. This should trigger 3
        // calls for plural strings.
        element.shadowRoot!.querySelector<HTMLElement>(
                               '#removeAllButton')!.click();
        await microtasksFinished();
        const completionText = pluralString.getArgs('getPluralString')[2];
        assertTrue(!!completionTextContainer);
        assertTrue(isVisible(completionTextContainer));
        assertEquals(completionText.messageName, 'safetyCheckAllDoneForNow');
        assertEquals(completionText.itemCount, 3);
      });

  test('CompletionStateShouldBeShownAfterKeepingItems', async function() {
    const completionTextContainer =
        element.shadowRoot!.querySelector('.completion-container');
    class MockKeepItemDelegate extends MockItemDelegate {
      override setItemSafetyCheckWarningAcknowledged(): void {
        // Update extensions to be an empty list since the only previous
        // extension was marked as acknowledged.
        element.extensions = [];
      }
    }
    element.delegate = new MockKeepItemDelegate();
    assertFalse(isVisible(completionTextContainer));
    const extensionRowContainers =
        element.shadowRoot!.querySelectorAll('.panel-extension-row');
    assertEquals(1, extensionRowContainers.length);
    const menuButton = extensionRowContainers[0]!.querySelector<HTMLElement>(
        '.icon-more-vert')!;
    const actionMenu = element.$.makeExceptionMenu;
    assertFalse(actionMenu.open);

    // Open the three dots action menu.
    menuButton.click();
    await microtasksFinished();
    // The three dots action menu should be open.
    assertTrue(actionMenu.open);

    // Click the Keep the Extension button.
    actionMenu.querySelector('button')!.click();
    await microtasksFinished();

    // The extension row should be removed and the completion state should be
    // shown.
    assertTrue(!!completionTextContainer);
    assertTrue(isVisible(completionTextContainer));
  });
});
