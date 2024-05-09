// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-mv2-deprecation-panel. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsMv2DeprecationPanelElement} from 'chrome://extensions/extensions.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createExtensionInfo} from './test_util.js';

suite('ExtensionsMV2DeprecationPanel', function() {
  let panelElement: ExtensionsMv2DeprecationPanelElement;

  setup(function() {
    panelElement = document.createElement('extensions-mv2-deprecation-panel');
    panelElement.extensions = [createExtensionInfo({
      name: 'Extension A',
      isAffectedByMV2Deprecation: true,
    })];
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
});
