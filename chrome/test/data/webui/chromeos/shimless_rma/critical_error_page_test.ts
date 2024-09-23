// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CriticalErrorPage} from 'chrome://shimless-rma/critical_error_page.js';
import {DISABLE_ALL_BUTTONS} from 'chrome://shimless-rma/events.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('criticalErrorPageTest', function() {
  let component: CriticalErrorPage|null = null;

  let service: FakeShimlessRmaService|null = null;

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

  function initializeCriticalErrorPage(): Promise<void> {
    assert(!component);
    component = document.createElement(CriticalErrorPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the buttons are disabled when the Exit to Login button is clicked.
  test('ClickExitToLoginButton', async () => {
    await initializeCriticalErrorPage();

    const resolver = new PromiseResolver<void>();
    assert(component);
    component.addEventListener(DISABLE_ALL_BUTTONS, () => {
      assert(component);
      component.allButtonsDisabled = true;
      resolver.resolve();
    });

    const exitToLoginButton = strictQuery(
        '#exitToLoginButton', component.shadowRoot, CrButtonElement);
    exitToLoginButton.click();

    await resolver.promise;
    assertTrue(component.allButtonsDisabled);
    assertTrue(exitToLoginButton.disabled);
  });

  // Verify the buttons are disabled when the Reboot button is clicked.
  test('ClickRebootButton', async () => {
    await initializeCriticalErrorPage();

    const resolver = new PromiseResolver<void>();
    assert(component);
    component.addEventListener(DISABLE_ALL_BUTTONS, () => {
      assert(component);
      component.allButtonsDisabled = true;
      resolver.resolve();
    });

    const rebootButton =
        strictQuery('#rebootButton', component.shadowRoot, CrButtonElement);
    rebootButton.click();

    await resolver.promise;
    assertTrue(component.allButtonsDisabled);
    assertTrue(rebootButton.disabled);
  });
});
