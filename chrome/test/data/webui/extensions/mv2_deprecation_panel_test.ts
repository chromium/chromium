// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-mv2-deprecation-panel. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsMv2DeprecationPanelElement} from 'chrome://extensions/extensions.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionsMV2DeprecationPanel', function() {
  let panelElement: ExtensionsMv2DeprecationPanelElement;
  let mockDelegate: TestService;

  setup(function() {
    mockDelegate = new TestService();

    panelElement = document.createElement('extensions-mv2-deprecation-panel');
    panelElement.extensions = [createExtensionInfo({
      name: 'Extension A',
      id: 'a'.repeat(32),
      isAffectedByMV2Deprecation: true,
      mustRemainInstalled: false,
    })];
    panelElement.delegate = mockDelegate;
    document.body.appendChild(panelElement);

    return flushTasks();
  });

  test('header content is always visible', function() {
    assertTrue(
        isVisible(panelElement.shadowRoot!.querySelector('.header-text')));
    assertTrue(
        isVisible(panelElement.shadowRoot!.querySelector('.header-button')));
  });

  test('correct number of extension rows', async function() {
    // Verify there is one extension row for the extension added at setup.
    let extensionRows =
        panelElement.shadowRoot!.querySelectorAll('.extension-row');
    assertEquals(extensionRows.length, 1);
    assertEquals(
        extensionRows[0]?.querySelector('.extension-name')?.textContent?.trim(),
        'Extension A');

    // Add a new extension to the panel.
    panelElement.push('extensions', createExtensionInfo({
                        name: 'Extension B',
                        isAffectedByMV2Deprecation: true,
                      }));
    await flushTasks();

    // Verify there are two extension rows.
    extensionRows = panelElement.shadowRoot!.querySelectorAll('.extension-row');
    assertEquals(extensionRows.length, 2);
    assertEquals(
        extensionRows[0]?.querySelector('.extension-name')?.textContent?.trim(),
        'Extension A');
    assertEquals(
        extensionRows[1]?.querySelector('.extension-name')?.textContent?.trim(),
        'Extension B');
  });

  test(
      'dismiss button triggers the warning dismissal when clicked',
      async function() {
        const dismissButton =
            panelElement.shadowRoot!.querySelector<CrButtonElement>(
                '.header-button');
        assertTrue(!!dismissButton);

        dismissButton.click();
        await mockDelegate.whenCalled('dismissMv2DeprecationWarning');
        assertEquals(
            1, mockDelegate.getCallCount('dismissMv2DeprecationWarning'));
      });

  test(
      'find alternative button is visible if extension has recommendations' +
          'url, and opens url when clicked',
      async function() {
        let extension = panelElement.shadowRoot!
                            .querySelectorAll<HTMLElement>('.extension-row')
                            ?.[0];
        assertTrue(!!extension);

        // Find alternative button is hidden when the extension doesn't have a
        // recommendations url.
        let findAlternativeButton = extension.querySelector<CrButtonElement>(
            '.find-alternative-button');
        assertFalse(isVisible(findAlternativeButton));

        // Add a recommendations url to the existent extension.
        const id = 'a'.repeat(32);
        const recommendationsUrl =
            `https://chromewebstore.google.com/detail/${id}` +
            `/related-recommendations`;
        panelElement.set('extensions.0', createExtensionInfo({
                           name: 'Extension A',
                           id,
                           isAffectedByMV2Deprecation: true,
                           recommendationsUrl,
                         }));
        await flushTasks();

        extension = panelElement.shadowRoot!
                        .querySelectorAll<HTMLElement>('.extension-row')
                        ?.[0];
        assertTrue(!!extension);

        // Find alternative button is visible when the extension has a
        // recommendations url.
        findAlternativeButton = extension.querySelector<CrButtonElement>(
            '.find-alternative-button');
        assertTrue(isVisible(findAlternativeButton));

        // Click on the find alternative button, and verify it triggered the
        // correct delegate call.
        findAlternativeButton?.click();
        await mockDelegate.whenCalled('openUrl');
        assertEquals(1, mockDelegate.getCallCount('openUrl'));
        assertDeepEquals([recommendationsUrl], mockDelegate.getArgs('openUrl'));
      });

  test(
      'remove action is visible if extension can be removed, and triggers' +
          'the extension removal when clicked',
      async function() {
        let extension = panelElement.shadowRoot!
                            .querySelectorAll<HTMLElement>('.extension-row')
                            ?.[0];
        assertTrue(!!extension);

        // Open the extension's action menu button.
        let actionButton =
            extension.querySelector<CrIconButtonElement>('cr-icon-button');
        assertTrue(!!actionButton);
        actionButton.click();

        // Remove button is visible when the extension doesn't need to remain
        // installed.
        let removeAction = panelElement.shadowRoot!.querySelector<HTMLElement>(
            '#removeAction');
        assertTrue(isVisible(removeAction));

        // Click on the remove button in the action menu, and verify it
        // triggered the correct delegate call.
        removeAction?.click();
        await mockDelegate.whenCalled('deleteItem');
        assertEquals(1, mockDelegate.getCallCount('deleteItem'));
        assertDeepEquals(
            [panelElement.extensions[0]?.id],
            mockDelegate.getArgs('deleteItem'));

        // Set the extension property to be force installed.
        panelElement.set('extensions.0', createExtensionInfo({
                           name: 'Extension A',
                           id: 'a'.repeat(32),
                           isAffectedByMV2Deprecation: true,
                           mustRemainInstalled: true,
                         }));
        await flushTasks();

        // Open the extension's action menu button again, since clicking on the
        // action closed the menu.
        extension = panelElement.shadowRoot!
                        .querySelectorAll<HTMLElement>('.extension-row')
                        ?.[0];
        assertTrue(!!extension);
        actionButton =
            extension.querySelector<CrIconButtonElement>('cr-icon-button');
        assertTrue(!!actionButton);
        actionButton.click();

        // Remove action is hidden when the extension must remain installed.
        removeAction = panelElement.shadowRoot!.querySelector<HTMLElement>(
            '#removeAction');
        assertFalse(isVisible(removeAction));
      });

  test(
      'keep action menu button triggers a warning dismissal for the extension' +
          'when clicked',
      async function() {
        const extension = panelElement.shadowRoot!
                              .querySelectorAll<HTMLElement>('.extension-row')
                              ?.[0];
        assertTrue(!!extension);

        // Open the extension's action menu button.
        const actionButton =
            extension.querySelector<CrIconButtonElement>('cr-icon-button');
        assertTrue(!!actionButton);
        actionButton.click();

        // Next, click on the "keep for now" button in the action menu, and
        // verify its own call.
        const keepAction =
            panelElement.shadowRoot!.querySelector<HTMLElement>('#keepAction');
        assertTrue(!!keepAction);
        keepAction.click();
        await mockDelegate.whenCalled(
            'dismissMv2DeprecationWarningForExtension');
        assertEquals(
            1,
            mockDelegate.getCallCount(
                'dismissMv2DeprecationWarningForExtension'));
        assertDeepEquals(
            [panelElement.extensions[0]?.id],
            mockDelegate.getArgs('dismissMv2DeprecationWarningForExtension'));
      });
});
