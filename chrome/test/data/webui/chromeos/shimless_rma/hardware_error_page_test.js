// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {HardwareErrorPage} from 'chrome://shimless-rma/hardware_error_page.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ShutdownMethod} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function hardwareErrorPageTest() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event used by the
   * shutdown button.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?HardwareErrorPage} */
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

  /**
   * @return {!Promise}
   */
  function initializeHardwareErrorPage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!HardwareErrorPage} */ (
        document.createElement('hardware-error-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ShutDownButtonDisablesAllButtons', async () => {
    await initializeHardwareErrorPage();

    const resolver = new PromiseResolver();
    let allButtonsDisabled = false;
    component.addEventListener('disable-all-buttons', (e) => {
      allButtonsDisabled = true;
      component.allButtonsDisabled = allButtonsDisabled;
      resolver.resolve();
    });

    component.shadowRoot.querySelector('#shutDownButton').click();

    await resolver.promise;
    assertTrue(allButtonsDisabled);
    assertTrue(component.shadowRoot.querySelector('#shutDownButton').disabled);
  });

  test('ShutDownButtonTriggersShutDown', async () => {
    const resolver = new PromiseResolver();
    await initializeHardwareErrorPage();

    let callCount = 0;
    service.shutDownAfterHardwareError = () => {
      callCount++;
      return resolver.promise;
    };
    await flushTasks();

    component.shadowRoot.querySelector('#shutDownButton').click();
    await flushTasks();

    assertEquals(1, callCount);
  });
}
