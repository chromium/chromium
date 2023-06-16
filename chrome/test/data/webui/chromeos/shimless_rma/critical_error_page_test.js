// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {CriticalErrorPage} from 'chrome://shimless-rma/critical_error_page.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('criticalErrorPageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event used by the
   * shutdown buttons.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?CriticalErrorPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
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
  function initializeCriticalErrorPage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!CriticalErrorPage} */ (
        document.createElement('critical-error-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the buttons are disabled when the Exit to Login button is clicked.
  test('ClickExitToLoginButton', async () => {
    await initializeCriticalErrorPage();
    assertTrue(!!component);

    const resolver = new PromiseResolver();
    let allButtonsDisabled = false;
    component.addEventListener('disable-all-buttons', (e) => {
      allButtonsDisabled = true;
      component.allButtonsDisabled = allButtonsDisabled;
      resolver.resolve();
    });

    component.shadowRoot.querySelector('#exitToLoginButton').click();

    await resolver.promise;
    assertTrue(allButtonsDisabled);
    assertTrue(
        component.shadowRoot.querySelector('#exitToLoginButton').disabled);
  });

  // Verify the buttons are disabled when the Reboot button is clicked.
  test('ClickRebootButton', async () => {
    await initializeCriticalErrorPage();
    assertTrue(!!component);

    const resolver = new PromiseResolver();
    let allButtonsDisabled = false;
    component.addEventListener('disable-all-buttons', (e) => {
      allButtonsDisabled = true;
      component.allButtonsDisabled = allButtonsDisabled;
      resolver.resolve();
    });

    component.shadowRoot.querySelector('#rebootButton').click();

    await resolver.promise;
    assertTrue(allButtonsDisabled);
    assertTrue(component.shadowRoot.querySelector('#rebootButton').disabled);
  });
});
