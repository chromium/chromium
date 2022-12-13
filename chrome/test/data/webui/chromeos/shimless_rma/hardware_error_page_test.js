// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {HardwareErrorPage} from 'chrome://shimless-rma/hardware_error_page.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ShutdownMethod} from 'chrome://shimless-rma/shimless_rma_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

/** @type {number} */
const ERROR_CODE = 1004;

suite('hardwareErrorPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event used by the
   * shutdown button.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

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
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeHardwareErrorPage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!HardwareErrorPage} */ (
        document.createElement('hardware-error-page'));
    component.errorCode = ERROR_CODE;
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

  test('ErrorCodeDisplayed', async () => {
    await initializeHardwareErrorPage();

    assertEquals(
        loadTimeData.getStringF('hardwareErrorCode', ERROR_CODE),
        component.shadowRoot.querySelector('#errorCode').textContent.trim());
  });
});
