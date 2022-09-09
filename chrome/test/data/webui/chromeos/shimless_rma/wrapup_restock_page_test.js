// Copyright 2021 The Chromium Authors
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

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    shimless_rma_component.remove();
    shimless_rma_component = null;
    service.reset();
  });

  /** @return {!Promise} */
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

  /** @return {!Promise} */
  function clickShutdownButton() {
    const shutdownComponent = component.shadowRoot.querySelector('#shutdown');
    assertTrue(!!shutdownComponent);
    shutdownComponent.click();
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

  test('RestockPageOnShutdownCallsShutdownForRestock', async () => {
    const resolver = new PromiseResolver();
    await initializeRestockPage();
    let restockCallCounter = 0;
    service.shutdownForRestock = () => {
      restockCallCounter++;
      return resolver.promise;
    };

    await clickShutdownButton();

    assertEquals(1, restockCallCounter);
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
