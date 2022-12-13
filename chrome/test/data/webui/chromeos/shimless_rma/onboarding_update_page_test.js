// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingUpdatePageElement} from 'chrome://shimless-rma/onboarding_update_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {OsUpdateOperation, UpdateErrorCode} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

suite('onboardingUpdatePageTest', function() {
  /** @type {?OnboardingUpdatePageElement} */
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
    const updateErrorDiv =
        component.shadowRoot.querySelector('#updateErrorDiv');
    assertTrue(updateErrorDiv.hidden);
    await clickPerformUpdateButton();

    service.triggerOsUpdateObserver(
        OsUpdateOperation.kDownloading, 0.5, UpdateErrorCode.kSuccess, 0);
    await flushTasks();

    assertTrue(updateInstructionsDiv.hidden);
    assertFalse(updateStatusDiv.hidden);
    assertTrue(updateErrorDiv.hidden);
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

  test('UpdatePageShowsErrors', async () => {
    const version = '90.1.2.3';
    await initializeUpdatePage(version);

    const updateInstructionsDiv =
        component.shadowRoot.querySelector('#updateInstructionsDiv');
    assertFalse(updateInstructionsDiv.hidden);
    const updateStatusDiv =
        component.shadowRoot.querySelector('#updateStatusDiv');
    assertTrue(updateStatusDiv.hidden);
    const updateErrorDiv =
        component.shadowRoot.querySelector('#updateErrorDiv');
    assertTrue(updateErrorDiv.hidden);
    await clickPerformUpdateButton();

    service.triggerOsUpdateObserver(
        OsUpdateOperation.kReportingErrorEvent, 0.5,
        UpdateErrorCode.kDownloadError, 0);
    await flushTasks();

    assertTrue(updateInstructionsDiv.hidden);
    assertTrue(updateStatusDiv.hidden);
    assertFalse(updateErrorDiv.hidden);
  });

  test('UpdatePageAllowsRetryAfterError', async () => {
    const version = '90.1.2.3';
    await initializeUpdatePage(version);

    // First, we need to get to the error screen.
    const updateInstructionsDiv =
        component.shadowRoot.querySelector('#updateInstructionsDiv');
    assertFalse(updateInstructionsDiv.hidden);
    const updateStatusDiv =
        component.shadowRoot.querySelector('#updateStatusDiv');
    assertTrue(updateStatusDiv.hidden);
    const updateErrorDiv =
        component.shadowRoot.querySelector('#updateErrorDiv');
    assertTrue(updateErrorDiv.hidden);
    await clickPerformUpdateButton();

    service.triggerOsUpdateObserver(
        OsUpdateOperation.kReportingErrorEvent, 0.5,
        UpdateErrorCode.kDownloadError, 0);
    await flushTasks();

    assertTrue(updateInstructionsDiv.hidden);
    assertTrue(updateStatusDiv.hidden);
    assertFalse(updateErrorDiv.hidden);

    // Next, click the Retry button.
    const retryUpdateButton =
        component.shadowRoot.querySelector('#retryUpdateButton');
    retryUpdateButton.click();
    await flushTasks();

    // This should send us back to the update progress screen.
    assertTrue(updateInstructionsDiv.hidden);
    assertFalse(updateStatusDiv.hidden);
    assertTrue(updateErrorDiv.hidden);
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
});
