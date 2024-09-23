// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {HardwareErrorPage} from 'chrome://shimless-rma/hardware_error_page.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const ERROR_CODE = 1004;

suite('hardwareErrorPageTest', function() {
  let component: HardwareErrorPage|null = null;

  let service: FakeShimlessRmaService|null = null;

  const shutDownButtonSelector = '#shutDownButton';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    service = null;
  });

  function initializeHardwareErrorPage(): Promise<void> {
    assert(!component);
    component = document.createElement(HardwareErrorPage.is);
    component.errorCode = ERROR_CODE;
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify clicking the Shutdown button disables all buttons.
  test('ShutDownButtonDisablesAllButtons', async () => {
    await initializeHardwareErrorPage();

    const resolver = new PromiseResolver<void>();
    assert(component);
    component.addEventListener('disable-all-buttons', () => {
      assert(component);
      component.allButtonsDisabled = true;
      resolver.resolve();
    });

    const shutDownButton = strictQuery(
        shutDownButtonSelector, component.shadowRoot, CrButtonElement);
    shutDownButton.click();

    await resolver.promise;
    assertTrue(component.allButtonsDisabled);
    assertTrue(shutDownButton.disabled);
  });

  // Verify clicking the Shutdown button triggers the appropriate service
  // endpoint.
  test('ShutDownButtonTriggersShutDown', async () => {
    const resolver = new PromiseResolver();
    await initializeHardwareErrorPage();

    let callCount = 0;
    assert(service);
    service.shutDownAfterHardwareError = () => {
      ++callCount;
      return resolver.promise;
    };
    await flushTasks();

    assert(component);
    strictQuery(shutDownButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    const expectedCallCount = 1;
    assertEquals(expectedCallCount, callCount);
  });

  // Verify error text is shown based on the current `errorCode`.
  test('ErrorCodeDisplayed', async () => {
    await initializeHardwareErrorPage();

    assert(component);
    const errorCodeText =
        strictQuery('#errorCode', component.shadowRoot, HTMLElement)
            .textContent;
    assert(errorCodeText);
    assertEquals(
        loadTimeData.getStringF('hardwareErrorCode', ERROR_CODE),
        errorCodeText.trim());
  });
});
