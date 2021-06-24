// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {WrapupRepairCompletePage} from 'chrome://shimless-rma/wrapup_repair_complete_page.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';
import {FakeNetworkConfig} from '../fake_network_config_mojom.m.js';

export function wrapupRepairCompletePageTest() {
  /** @type {?WrapupRepairCompletePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let shimlessRmaService = null;

  suiteSetup(() => {
    shimlessRmaService = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(shimlessRmaService);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    shimlessRmaService.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeRepairCompletePage() {
    assertFalse(!!component);

    component = /** @type {!WrapupRepairCompletePage} */ (
        document.createElement('wrapup-repair-complete-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @param {string} buttonNameSelector
   * @return {!Promise}
   */
  function clickButton(buttonNameSelector) {
    assertTrue(!!component);

    const button = component.shadowRoot.querySelector(buttonNameSelector);
    button.click();
    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeRepairCompletePage();
    assertTrue(!!component);

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);

    const batteryDialog =
        component.shadowRoot.querySelector('#batteryCutDialog');
    assertTrue(!!batteryDialog);
    assertFalse(batteryDialog.open);
  });


  test('OpensRmaLogDialog', async () => {
    await initializeRepairCompletePage();
    await clickButton('#rmaLogButton');

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertTrue(logsDialog.open);
  });

  test('OpensBatteryCutDialog', async () => {
    await initializeRepairCompletePage();
    await clickButton('#batteryCutButton');

    const batteryDialog =
        component.shadowRoot.querySelector('#batteryCutDialog');
    assertTrue(!!batteryDialog);
    assertTrue(batteryDialog.open);
  });

  test('DialogCloses', async () => {
    await initializeRepairCompletePage();
    await clickButton('#rmaLogButton');
    await clickButton('#closeLogDialogButton');

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);

    await clickButton('#batteryCutButton');
    await clickButton('#closeBatteryDialogButton');

    const batteryDialog =
        component.shadowRoot.querySelector('#batteryCutDialog');
    assertTrue(!!batteryDialog);
    assertFalse(batteryDialog.open);
  });
}
