// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {getShimlessRmaService, setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingUpdatePageElement} from 'chrome://shimless-rma/onboarding_update_page.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingUpdatePageTest() {
  /** @type {?OnboardingUpdatePageElement} */
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
    service.reset();
  });

  /**
   * @param {string} version
   * @param {boolean} updateAvailable
   * @return {!Promise}
   */
  function initializeUpdatePage(version, updateAvailable) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setGetCurrentOsVersionResult(version);
    service.setCheckForOsUpdatesResult(updateAvailable);

    component = /** @type {!OnboardingUpdatePageElement} */ (
        document.createElement('onboarding-update-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickCheckUpdateButton() {
    const checkUpdateButton =
        component.shadowRoot.querySelector('#checkUpdate');
    checkUpdateButton.click();
    return flushTasks();
  }

  test('UpdatePageInitializes', () => {
    const version = '90.1.2.3';
    const update = false;

    return initializeUpdatePage(version, update).then(() => {
      const versionComponent =
          component.shadowRoot.querySelector('#versionInfo');
      const updateButton = component.shadowRoot.querySelector('#performUpdate');
      assertTrue(versionComponent.textContent.trim().indexOf(version) !== -1);
      assertTrue(updateButton.hidden);
    });
  });

  test('UpdatePageNoNetwork', () => {
    const version = '90.1.2.3';
    const update = false;

    return initializeUpdatePage(version, update)
        .then(() => {
          component.networkAvailable = false;
          return flushTasks();
        })
        .then(() => {
          const networkUnavailable =
              component.shadowRoot.querySelector('#networkUnavailable');
          const checkUpdateButton =
              component.shadowRoot.querySelector('#checkUpdate');

          assertFalse(networkUnavailable.hidden);
          assertTrue(checkUpdateButton.hidden);
        });
  });

  test('UpdatePageNoUpdates', () => {
    const version = '90.1.2.3';
    const update = false;

    return initializeUpdatePage(version, update)
        .then(() => {
          component.networkAvailable = true;
          return flushTasks();
        })
        .then(() => clickCheckUpdateButton())
        .then(() => {
          const versionComponent =
              component.shadowRoot.querySelector('#versionInfo');
          // TODO(joonbug): update with i18n string
          const uptoDateMsg = 'is up to date';
          assertTrue(
              versionComponent.textContent.trim().indexOf(uptoDateMsg) !== -1);
        });
  });

  test('UpdatePageUpdatesAvailable', () => {
    const version = '90.1.2.3';
    const update = true;

    return initializeUpdatePage(version, update)
        .then(() => {
          component.networkAvailable = true;
          return flushTasks();
        })
        .then(() => clickCheckUpdateButton())
        .then(() => {
          const versionComponent =
              component.shadowRoot.querySelector('#versionInfo');
          // TODO(joonbug): update with i18n string
          const uptoDateMsg = 'is out of date';
          assertTrue(
              versionComponent.textContent.trim().indexOf(uptoDateMsg) !== -1);
        })
        .then(() => {
          const updateButton =
              component.shadowRoot.querySelector('#performUpdate');
          assertFalse(updateButton.hidden);
        });
  });
}
