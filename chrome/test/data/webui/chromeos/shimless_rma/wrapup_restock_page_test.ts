// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {WrapupRestockPage} from 'chrome://shimless-rma/wrapup_restock_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('wrapupRestockPageTest', function() {
  // ShimlessRma is needed to handle the 'transition-state' event used when
  // handling the continue or shutdown actions.
  let shimlessRmaComponent: ShimlessRma|null = null;

  let component: WrapupRestockPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  const shutdownButtonSelector = '#shutdown';
  const continueButtonSelector = '#continue';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
  });

  function initializeRestockPage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(WrapupRestockPage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  function clickShutdownButton(): Promise<void> {
    assert(component);
    strictQuery(shutdownButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  // Verify component renders.
  test('ComponentRenders', async () => {
    await initializeRestockPage();

    assert(component);
    const basePage =
        strictQuery('base-page', component.shadowRoot, HTMLElement);
    assert(basePage);
  });

  // Verify clicking the next button continues finalization.
  test('NextButtonContinueFinalization', async () => {
    await initializeRestockPage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCounter = 0;
    service.continueFinalizationAfterRestock = () => {
      ++callCounter;
      return resolver.promise;
    };

    assert(component);
    strictQuery(continueButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    await resolver;
    assertEquals(1, callCounter);
  });

  // Verify clicking the shutdown button shuts down device for restock.
  test('ShutdownButtonShutsDownDevice', async () => {
    await initializeRestockPage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let restockCallCounter = 0;
    service.shutdownForRestock = () => {
      ++restockCallCounter;
      return resolver.promise;
    };

    await clickShutdownButton();
    assertEquals(1, restockCallCounter);
  });

  // Verify when `allButtonsDisabled` is set all buttons are disabled.
  test('RestockPageButtonsDisabled', async () => {
    await initializeRestockPage();

    assert(component);
    const continueButton = strictQuery(
        continueButtonSelector, component.shadowRoot, CrButtonElement);
    const shutdownButton = strictQuery(
        shutdownButtonSelector, component.shadowRoot, CrButtonElement);
    assertFalse(continueButton.disabled);
    assertFalse(shutdownButton.disabled);

    component.allButtonsDisabled = true;
    assertTrue(continueButton.disabled);
    assertTrue(shutdownButton.disabled);
  });
});
