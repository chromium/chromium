// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RepairComponentChipElement} from 'chrome://shimless-rma/repair_component_chip.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function repairComponentChipElementTest() {
  /** @type {?RepairComponentChipElement} */
  let component = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
  });

  /**
   * @param {string} componentName
   * @return {!Promise}
   */
  function initializeRepairComponentChip(componentName) {
    assertFalse(!!component);

    component = /** @type {!RepairComponentChipElement} */ (
        document.createElement('repair-component-chip'));
    component.componentName = componentName;
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickChip() {
    assertTrue(!!component);

    const button = component.shadowRoot.querySelector('#containerButton');
    button.click();
    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeRepairComponentChip('cpu');
    assertTrue(!!component);
    assertFalse(component.checked);

    const componentNameSpanElement =
        component.shadowRoot.querySelector('#componentName');
    assertTrue(!!componentNameSpanElement);
    assertEquals(componentNameSpanElement.textContent, 'cpu');
  });

  test('ComponentToggleChecked', async () => {
    await initializeRepairComponentChip('cpu');

    await clickChip();
    assertTrue(component.checked);

    await clickChip();
    assertFalse(component.checked);
  });

  test('ComponentNoToggleOnDisabled', async () => {
    await initializeRepairComponentChip('cpu');
    component.disabled = true;
    await flushTasks();
    await clickChip();

    assertFalse(component.checked);
  });
}
