// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {RepairComponentChip} from 'chrome://shimless-rma/repair_component_chip.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('repairComponentChipTest', function() {
  let component: RepairComponentChip|null = null;

  const chipName = 'cpu';
  const chipIdentifier = 'cpu_123';

  const checkIconIdentifier = '#checkIcon';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function initializeRepairComponentChip(): Promise<void> {
    assert(!component);
    component = document.createElement(RepairComponentChip.is);
    component.componentName = chipName;
    component.componentIdentifier = chipIdentifier;
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  function clickChip(): Promise<void> {
    assert(component);
    strictQuery('#componentButton', component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  // Verify the chip component renders with the expected attributes.
  test('ComponentRenders', async () => {
    await initializeRepairComponentChip();

    assert(component);
    assertFalse(component.checked);
    assertEquals(
        chipName,
        strictQuery('#componentName', component.shadowRoot, HTMLElement)
            .textContent);
    assertEquals(
        chipIdentifier,
        strictQuery('#componentIdentifier', component.shadowRoot, HTMLElement)
            .textContent);
  });

  // Verify the checked attribute toggles when clicked.
  test('ComponentToggleCheckedOnClick', async () => {
    await initializeRepairComponentChip();

    assert(component);
    const checkIcon =
        strictQuery(checkIconIdentifier, component.shadowRoot, HTMLElement);
    await clickChip();
    assertTrue(component.checked);
    assertTrue(isVisible(checkIcon));

    await clickChip();
    assertFalse(component.checked);
    assertFalse(isVisible(checkIcon));

    // When disabled the chip should not check when clicked.
    component.disabled = true;
    await clickChip();
    assertFalse(component.checked);
    assertFalse(isVisible(checkIcon));
  });
});
