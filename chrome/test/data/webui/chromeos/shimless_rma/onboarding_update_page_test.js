// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingUpdatePageElement} from 'chrome://shimless-rma/onboarding_update_page.js';
import {OsUpdateOperation} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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
    service.setCheckForOsUpdatesResult(updateAvailable, 'fake version');
    service.setUpdateOsResult(updateAvailable);

    component = /** @type {!OnboardingUpdatePageElement} */ (
        document.createElement('onboarding-update-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function clickPerformUpdateButton() {
    const performUpdateButton =
        component.shadowRoot.querySelector('#performUpdateButton');
    performUpdateButton.click();
    return flushTasks();
  }

  test('UpdatePageInitializes', () => {
    const version = '90.1.2.3';
    const update = false;

    return initializeUpdatePage(version, update).then(() => {
      const versionComponent =
          component.shadowRoot.querySelector('#versionInfo');
      const updateButton =
          component.shadowRoot.querySelector('#performUpdateButton');
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
          const updateButton =
              component.shadowRoot.querySelector('#performUpdateButton');

          assertFalse(networkUnavailable.hidden);
          assertTrue(updateButton.hidden);
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
        .then(() => {
          const versionComponent =
              component.shadowRoot.querySelector('#versionInfo');
          // TODO(gavindodd): update with i18n string
          const uptoDateMsg = 'is out of date';
          assertTrue(
              versionComponent.textContent.trim().indexOf(uptoDateMsg) !== -1);
        })
        .then(() => {
          const updateButton =
              component.shadowRoot.querySelector('#performUpdateButton');
          assertFalse(updateButton.hidden);
        });
  });

  test('UpdatePageUpdateStarts', () => {
    const version = '90.1.2.3';
    const update = true;

    return initializeUpdatePage(version, update)
        .then(() => {
          component.networkAvailable = true;
          return flushTasks();
        })
        .then(() => {
          const versionComponent =
              component.shadowRoot.querySelector('#versionInfo');
          // TODO(gavindodd): update with i18n string
          const uptoDateMsg = 'is out of date';
          assertTrue(
              versionComponent.textContent.trim().indexOf(uptoDateMsg) !== -1);
          const updateButton =
              component.shadowRoot.querySelector('#performUpdateButton');
          assertFalse(updateButton.disabled);
          assertFalse(updateButton.hidden);
        })
        .then(() => clickPerformUpdateButton())
        .then(() => {
          // A successfully started update should disable the update button.
          const updateButton =
              component.shadowRoot.querySelector('#performUpdateButton');
          assertTrue(updateButton.disabled);
        });
  });

  test('UpdatePageShowsUpdateProgress', async () => {
    const version = '90.1.2.3';
    const update = true;
    await initializeUpdatePage(version, update);

    const progressComponent =
        component.shadowRoot.querySelector('#progressMessage');
    assertEquals('', progressComponent.textContent.trim());
    await clickPerformUpdateButton();

    service.triggerOsUpdateObserver(OsUpdateOperation.kDownloading, 0.5, 0);
    await flushTasks();

    // TODO(gavindodd): update with i18n string
    assertTrue(progressComponent.textContent.trim().startsWith(
        'OS update progress received '));
  });

  test('UpdatePageSetSkipButton', async () => {
    const version = '90.1.2.3';
    const update = false;

    service.setGetCurrentOsVersionResult(version);
    service.setCheckForOsUpdatesResult(update, 'fake version');
    service.setUpdateOsResult(update);

    component = /** @type {!OnboardingUpdatePageElement} */ (
        document.createElement('onboarding-update-page'));
    let buttonLabelKey;
    component.addEventListener('set-next-button-label', (e) => {
      buttonLabelKey = e.detail;
    });

    document.body.appendChild(component);
    await flushTasks();
    assertEquals('nextButtonLabel', buttonLabelKey);

    service.setCheckForOsUpdatesResult(true, 'fake version');
    component = /** @type {!OnboardingUpdatePageElement} */ (
        document.createElement('onboarding-update-page'));
    component.addEventListener('set-next-button-label', (e) => {
      buttonLabelKey = e.detail;
    });

    document.body.appendChild(component);
    await flushTasks();
    assertEquals('skipButtonLabel', buttonLabelKey);
  });
}
