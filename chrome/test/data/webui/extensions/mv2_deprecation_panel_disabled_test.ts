// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-mv2-deprecation-panel. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsMv2DeprecationPanelElement} from 'chrome://extensions/extensions.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionsMV2DeprecationPanel_DisabledStage', function() {
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
    // Stage 2 represents Mv2ExperimentStage.DISABLE_WITH_REENABLE.
    panelElement.mv2ExperimentStage = 2;
    panelElement.delegate = mockDelegate;
    document.body.appendChild(panelElement);

    return flushTasks();
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
    // TODO(crbug.com/339061151): header button should be visible once dismiss
    // panel for disabled stage is added.
    assertFalse(
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
    panelElement.push('extensions', createExtensionInfo({
                        name: 'Extension B',
                        isAffectedByMV2Deprecation: true,
                      }));
    await flushTasks();

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

  test('find alternative button for extension is hidden', function() {
    const extension = getExtension();
    const findAlternativeButton =
        extension.querySelector<CrButtonElement>('.find-alternative-button');
    assertFalse(isVisible(findAlternativeButton));
  });

  // TODO(crbug.com/339061151): Action menu button should be visible in the
  // disabled stage, once functionality is added.
  test('action menu button for extension is hidden', function() {
    const extension = getExtension();
    const actionButton =
        extension.querySelector<CrIconButtonElement>('cr-icon-button');
    assertFalse(isVisible(actionButton));
  });
});
