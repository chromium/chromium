// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingUpdatePageElement} from 'chrome://shimless-rma/onboarding_update_page.js';
import {OsUpdateOperation} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

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
   * @return {!Promise}
   */
  function initializeUpdatePage(version) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setGetCurrentOsVersionResult(version);
    service.setCheckForOsUpdatesResult('fake version');
    service.setUpdateOsResult(true);

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
    return initializeUpdatePage(version).then(() => {
      const versionComponent =
          component.shadowRoot.querySelector('#versionInfo');
      const updateButton =
          component.shadowRoot.querySelector('#performUpdateButton');
      assertTrue(versionComponent.textContent.trim().indexOf(version) !== -1);
    });
  });

  test('UpdatePageUpdatesAvailable', () => {
    const version = '90.1.2.3';
    return initializeUpdatePage(version).then(() => {
      const versionComponent =
          component.shadowRoot.querySelector('#versionInfo');
      assertEquals(
          loadTimeData.getStringF('currentVersionOutOfDateText', version),
          versionComponent.textContent.trim());
      const updateButton =
          component.shadowRoot.querySelector('#performUpdateButton');
      assertFalse(updateButton.hidden);
    });
  });

  test('UpdatePageUpdateStarts', () => {
    const version = '90.1.2.3';

    return initializeUpdatePage(version)
        .then(() => {
          component.addEventListener('disable-all-buttons', (e) => {
            component.allButtonsDisabled = true;
          });

          return flushTasks();
        })
        .then(() => {
          clickPerformUpdateButton();
        })
        .then(() => {
          // A successfully started update should disable the update button.
          const updateButton =
              component.shadowRoot.querySelector('#performUpdateButton');
          assertTrue(updateButton.disabled);
        });
  });

  test('UpdatePageShowsUpdateProgress', async () => {
    const version = '90.1.2.3';
    await initializeUpdatePage(version);

    const updateInstructionsDiv =
        component.shadowRoot.querySelector('#updateInstructionsDiv');
    assertFalse(updateInstructionsDiv.hidden);
    const updateStatusDiv =
        component.shadowRoot.querySelector('#updateStatusDiv');
    assertTrue(updateStatusDiv.hidden);
    await clickPerformUpdateButton();

    service.triggerOsUpdateObserver(OsUpdateOperation.kDownloading, 0.5, 0);
    await flushTasks();

    assertTrue(updateInstructionsDiv.hidden);
    assertFalse(updateStatusDiv.hidden);
  });

  test('UpdatePageShowHideUnqualifiedComponentsLink', () => {
    const version = '90.1.2.3';

    return initializeUpdatePage(version)
        .then(() => {
          service.triggerHardwareVerificationStatusObserver(true, '', 0);
          return flushTasks();
        })
        .then(() => {
          assertFalse(isVisible(component.shadowRoot.querySelector(
              '#unqualifiedComponentsLink')));
        });
  });

  test('UpdatePageShowLinkOpenDialogOnError', () => {
    const version = '90.1.2.3';
    const failedComponent = 'Keyboard';

    return initializeUpdatePage(version)
        .then(() => {
          service.triggerHardwareVerificationStatusObserver(
              false, failedComponent, 0);
          return flushTasks();
        })
        .then(() => {
          const unqualifiedComponentsLink =
              component.shadowRoot.querySelector('#unqualifiedComponentsLink');
          assertTrue(isVisible(unqualifiedComponentsLink));
          unqualifiedComponentsLink.click();

          assertTrue(
              component.shadowRoot.querySelector('#unqualifiedComponentsDialog')
                  .open);
          assertEquals(
              failedComponent,
              component.shadowRoot.querySelector('#dialogBody')
                  .textContent.trim());
        });
  });

  test('UpdatePageUpdateFailedToStartButtonsEnabled', () => {
    const version = '90.1.2.3';

    const resolver = new PromiseResolver();
    service.updateOs = () => {
      return resolver.promise;
    };

    return initializeUpdatePage(version).then(() => {
      let allButtonsDisabled = false;
      component.addEventListener('disable-all-buttons', (e) => {
        allButtonsDisabled = true;
      });
      component.addEventListener('enable-all-buttons', (e) => {
        allButtonsDisabled = false;
      });

      return clickPerformUpdateButton()
          .then(() => {
            // All buttons should be disabled when the update starts.
            assertTrue(allButtonsDisabled);
            resolver.resolve({updateStarted: false});
            return flushTasks();
          })
          .then(() => {
            // When the update fails, all the buttons should be re-enabled.
            assertFalse(allButtonsDisabled);
          });
    });
  });
}
