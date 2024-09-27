// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-mv2-deprecation-panel. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsMv2DeprecationPanelElement} from 'chrome://extensions/extensions.js';
import {Mv2ExperimentStage} from 'chrome://extensions/extensions.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionsMV2DeprecationPanel_WarningStage', function() {
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
    panelElement.mv2ExperimentStage = Mv2ExperimentStage.WARNING;
    panelElement.delegate = mockDelegate;
    document.body.appendChild(panelElement);

    return microtasksFinished();
  });

  /**
   * Returns the only extension in the panel. Will fail if there are no
   * extensions, or more than one.
   */
  function getExtension(): Element {
    const extensionRows =
        panelElement.shadowRoot!.querySelectorAll('.panel-extension-row');
    assertEquals(1, extensionRows.length);
    const extension = extensionRows[0];
    assertTrue(!!extension);
    return extension;
  }

  test('header content is always visible', function() {
    assertTrue(isVisible(
        panelElement.shadowRoot!.querySelector('.panel-header-text')));
    assertTrue(
        isVisible(panelElement.shadowRoot!.querySelector('.header-button')));
  });

  test('correct number of extension rows', async function() {
    // Verify there is one extension row for the extension added at setup.
    let extensionRows =
        panelElement.shadowRoot!.querySelectorAll('.panel-extension-row');
    assertEquals(1, extensionRows.length);
    let infoA =
        extensionRows[0]!.querySelector<HTMLElement>('.panel-extension-info');
    assertTrue(!!infoA);
    assertEquals('Extension A', infoA.textContent!.trim());

    // Add a new extension to the panel.
    panelElement.extensions = [
      ...panelElement.extensions,
      createExtensionInfo({
        name: 'Extension B',
        isAffectedByMV2Deprecation: true,
      }),
    ];
    await microtasksFinished();

    // Verify there are two extension rows.
    extensionRows =
        panelElement.shadowRoot!.querySelectorAll('.panel-extension-row');
    assertEquals(extensionRows.length, 2);
    infoA =
        extensionRows[0]!.querySelector<HTMLElement>('.panel-extension-info');
    assertTrue(!!infoA);
    assertEquals('Extension A', infoA.textContent!.trim());
    const infoB =
        extensionRows[1]!.querySelector<HTMLElement>('.panel-extension-info');
    assertTrue(!!infoB);
    assertEquals('Extension B', infoB.textContent!.trim());
  });

  test(
      'dismiss button triggers the warning dismissal when clicked',
      async function() {
        const dismissButton =
            panelElement.shadowRoot!.querySelector<CrButtonElement>(
                '.header-button');
        assertTrue(!!dismissButton);

        dismissButton.click();
        await mockDelegate.whenCalled('dismissMv2DeprecationNotice');
        assertEquals(
            1, mockDelegate.getCallCount('dismissMv2DeprecationNotice'));
      });

  test(
      'find alternative button is visible if extension has recommendations' +
          'url, and opens url when clicked',
      async function() {
        // Find alternative button is hidden when the extension doesn't have a
        // recommendations url.
        let extension = getExtension();
        let findAlternativeButton = extension.querySelector<CrButtonElement>(
            '.find-alternative-button');
        assertFalse(isVisible(findAlternativeButton));

        // Add a recommendations url to the existent extension.
        const id = 'a'.repeat(32);
        const recommendationsUrl =
            `https://chromewebstore.google.com/detail/${id}` +
            `/related-recommendations`;
        panelElement.extensions = [createExtensionInfo({
          name: 'Extension A',
          id,
          isAffectedByMV2Deprecation: true,
          recommendationsUrl,
        })];
        await microtasksFinished();

        // Find alternative button is visible when the extension has a
        // recommendations url.
        extension = getExtension();
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

  test('remove button is always hidden', function() {
    const extension = getExtension();
    const removeButton =
        extension.querySelector<CrIconButtonElement>('#removeButton');
    assertFalse(isVisible(removeButton));
  });

  test('find alternative action is always hidden', async () => {
    // Open the extension's action menu.
    const extension = getExtension();
    const actionButton =
        extension.querySelector<CrIconButtonElement>('#actionMenuButton');
    assertTrue(!!actionButton);
    actionButton.click();
    await microtasksFinished();

    // Find alternative action is always hidden.
    const findAlternativeAction =
        panelElement.shadowRoot!.querySelector<HTMLElement>(
            '#findAlternativeAction');
    assertFalse(isVisible(findAlternativeAction));
  });

  test(
      'remove action is visible if extension can be removed, and triggers' +
          'the extension removal when clicked',
      async function() {
        // Open the extension's action menu button.
        let extension = getExtension();
        let actionButton =
            extension.querySelector<CrIconButtonElement>('#actionMenuButton');
        assertTrue(!!actionButton);
        actionButton.click();
        await microtasksFinished();

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
        panelElement.extensions = [createExtensionInfo({
          name: 'Extension A',
          id: 'a'.repeat(32),
          isAffectedByMV2Deprecation: true,
          mustRemainInstalled: true,
        })];
        await microtasksFinished();

        // Open the extension's action menu button again, since clicking on the
        // action closed the menu.
        extension = getExtension();
        actionButton =
            extension.querySelector<CrIconButtonElement>('#actionMenuButton');
        assertTrue(!!actionButton);
        actionButton.click();
        await microtasksFinished();

        // Remove action is hidden when the extension must remain installed.
        removeAction = panelElement.shadowRoot!.querySelector<HTMLElement>(
            '#removeAction');
        assertFalse(isVisible(removeAction));
      });

  test(
      'keep action menu button triggers a warning dismissal for the extension' +
          'when clicked',
      async function() {
        // Open the extension's action menu.
        const extension = getExtension();
        const actionButton =
            extension.querySelector<CrIconButtonElement>('#actionMenuButton');
        assertTrue(!!actionButton);
        actionButton.click();
        await microtasksFinished();

        // Keep action is always visible.
        const keepAction =
            panelElement.shadowRoot!.querySelector<HTMLElement>('#keepAction');
        assertTrue(!!keepAction);
        assertTrue(isVisible(keepAction));

        // Click on keep action and verify it triggers the correct call.
        keepAction.click();
        await mockDelegate.whenCalled(
            'dismissMv2DeprecationNoticeForExtension');
        assertEquals(
            1,
            mockDelegate.getCallCount(
                'dismissMv2DeprecationNoticeForExtension'));
        assertDeepEquals(
            [panelElement.extensions[0]?.id],
            mockDelegate.getArgs('dismissMv2DeprecationNoticeForExtension'));
      });
});
