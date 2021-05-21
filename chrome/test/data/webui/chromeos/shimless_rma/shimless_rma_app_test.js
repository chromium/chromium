// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeChromeVersion, fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRmaElement} from 'chrome://shimless-rma/shimless_rma.js';
import {State} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

export function shimlessRMAAppTest() {
  /** @type {?ShimlessRmaElement} */
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
    document.body.innerHTML = '';
    service.reset();
  });

  /**
   * @param {!Array<!State>} states
   * @param {string} chromeVersion
   */
  function initializeShimlessRMAApp(states, chromeVersion) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setStates(states);
    service.setGetCurrentChromeVersionResult(chromeVersion);

    component = /** @type {!ShimlessRmaElement} */ (
        document.createElement('shimless-rma'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * Utility function to assert navigation buttons
   * TODO(joonbug): expand to cover assertion of BtnState
   */
  function assertNavButtons() {
    const nextBtn = component.shadowRoot.querySelector('#back');
    const prevBtn = component.shadowRoot.querySelector('#cancel');
    const backBtn = component.shadowRoot.querySelector('#next');
    assertTrue(!!nextBtn);
    assertTrue(!!prevBtn);
    assertTrue(!!backBtn);
  }

  test('ShimlessRMALoaded', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    assertNavButtons();
  });

  test('ShimlessRMABasicNavigation', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);
    assertFalse(initialPage.hidden);

    const nextBtn = component.shadowRoot.querySelector('#next');
    nextBtn.click();
    await flushTasks();

    // TODO(joonbug): enable when page is ready.
    // const selectComponentPage =
    //     component.shadowRoot.querySelector('onboarding-select-components');
    // assertTrue(!!selectComponentPage);
    // assertFalse(selectComponentPage.hidden);
    assertTrue(!!initialPage);  // initial page should not be destroyed on nav.
    assertTrue(initialPage.hidden);

    const prevBtn = component.shadowRoot.querySelector('#back');
    prevBtn.click();
    await flushTasks();

    // components page should not be destroyed.
    // assertTrue(!!selectComponentPage);
    // assertTrue(selectComponentPage.hidden);
    assertFalse(initialPage.hidden);
  });

  test('ShimlessRMACancellation', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    const nextBtn = component.shadowRoot.querySelector('#next');
    nextBtn.click();
    await flushTasks();

    const cancelBtn = component.shadowRoot.querySelector('#cancel');
    cancelBtn.click();
    await flushTasks();

    // back to initial page
    assertFalse(initialPage.hidden);
  });
}
