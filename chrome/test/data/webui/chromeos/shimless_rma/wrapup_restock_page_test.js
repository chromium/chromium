// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {WrapupRestockPage} from 'chrome://shimless-rma/wrapup_restock_page.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function wrapupRestockPageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used by
   * the shutdown button.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?WrapupRestockPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  suiteSetup(() => {
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    shimless_rma_component.remove();
    shimless_rma_component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeRestockPage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!WrapupRestockPage} */ (
        document.createElement('wrapup-restock-page'));
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
    await initializeRestockPage();
    assertTrue(!!component);

    const basePage = component.shadowRoot.querySelector('base-page');
    assertTrue(!!basePage);
  });

  test('RestockPageOnNextCallsContinueFinalizationAfterRestock', async () => {
    const resolver = new PromiseResolver();
    await initializeRestockPage();
    let callCounter = 0;
    service.continueFinalizationAfterRestock = () => {
      callCounter++;
      return resolver.promise;
    };

    component.shadowRoot.querySelector('#continue').click();
    await resolver;

    assertEquals(1, callCounter);
  });

  test('RestockPagePowerwashButtonCallsShutdownForRestock', async () => {
    const resolver = new PromiseResolver();
    await initializeRestockPage();
    let restockCallCounter = 0;
    service.shutdownForRestock = () => {
      restockCallCounter++;
      return resolver.promise;
    };

    await clickButton('#powerwashButton');

    assertEquals(1, restockCallCounter);
  });

  test('ShutDownButtonOpensPowerwashDialog', async () => {
    const resolver = new PromiseResolver();
    await initializeRestockPage();

    let callCount = 0;
    service.shutdownForRestock = () => {
      callCount++;
      return resolver.promise;
    };
    await flushTasks();

    await clickButton('#shutdown');

    // Don't shut down immediately.
    assertEquals(0, callCount);
    // Show the dialog instead.
    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);
    assertTrue(powerwashDialog.open);
  });

  test('CancelButtonClosesPowerwashDialog', async () => {
    await initializeRestockPage();
    await flushTasks();
    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);

    await clickButton('#shutdown');
    assertTrue(powerwashDialog.open);

    await clickButton('#closePowerwashDialogButton');
    assertFalse(powerwashDialog.open);
  });

  test('PowerwashDialogClosesWhenCompleted', async () => {
    await initializeRestockPage();

    const powerwashDialog =
        component.shadowRoot.querySelector('#powerwashDialog');
    assertTrue(!!powerwashDialog);

    await clickButton('#shutdown');
    assertTrue(powerwashDialog.open);

    await clickButton('#powerwashButton');
    assertFalse(powerwashDialog.open);
  });

  test('RestockPageButtonsDisabled', async () => {
    await initializeRestockPage();

    const continueButton = component.shadowRoot.querySelector('#continue');
    const shutdownButton = component.shadowRoot.querySelector('#shutdown');
    assertFalse(continueButton.disabled);
    assertFalse(shutdownButton.disabled);
    component.allButtonsDisabled = true;
    assertTrue(continueButton.disabled);
    assertTrue(shutdownButton.disabled);
  });
}
