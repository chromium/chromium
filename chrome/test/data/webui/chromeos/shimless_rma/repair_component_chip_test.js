// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RepairComponentChip} from 'chrome://shimless-rma/repair_component_chip.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

suite('repairComponentChipTest', function() {
  /** @type {?RepairComponentChip} */
  let component = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
  });

  teardown(() => {
    component.remove();
    component = null;
  });

  /**
   * @param {string} componentName
   * @param {string} componentIdentifier
   * @return {!Promise}
   */
  function initializeRepairComponentChip(componentName, componentIdentifier) {
    assertFalse(!!component);

    component = /** @type {!RepairComponentChip} */ (
        document.createElement('repair-component-chip'));
    component.componentName = componentName;
    component.componentIdentifier = componentIdentifier;
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickChip() {
    assertTrue(!!component);
    component.shadowRoot.querySelector('#componentButton').click();
    return flushTasks();
  }

  test('ComponentRenders', async () => {
    const name = 'cpu';
    const identifier = 'cpu_123';
    await initializeRepairComponentChip(name, identifier);
    assertTrue(!!component);
    assertFalse(component.checked);

    const nameElement = component.shadowRoot.querySelector('#componentName');
    assertTrue(!!nameElement);
    assertEquals(nameElement.textContent, name);

    const identifierElement =
        component.shadowRoot.querySelector('#componentIdentifier');
    assertTrue(!!identifierElement);
    assertEquals(identifierElement.textContent, identifier);
  });

  test('ComponentToggleCheckedOnClick', async () => {
    const name = 'cpu';
    const identifier = 'cpu_123';
    await initializeRepairComponentChip(name, identifier);

    const checkIcon = component.shadowRoot.querySelector('#checkIcon');

    await clickChip();
    assertTrue(component.checked);
    assertTrue(isVisible(checkIcon));

    await clickChip();
    assertFalse(component.checked);
    assertFalse(isVisible(checkIcon));
  });

  test('ComponentNoToggleOnDisabled', async () => {
    const name = 'cpu';
    const identifier = 'cpu_123';
    await initializeRepairComponentChip(name, identifier);
    component.disabled = true;
    await flushTasks();

    const checkIcon = component.shadowRoot.querySelector('#checkIcon');

    assertFalse(component.checked);
    assertFalse(isVisible(checkIcon));

    await clickChip();

    // Confirm the state does not change after the attempted click.
    assertFalse(component.checked);
    assertFalse(isVisible(checkIcon));
  });
});
