// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {fakeCalibrationComponentsWithFails, fakeChromeVersion, fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ButtonState, ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {RmadErrorCode, State, StateResult} from 'chrome://shimless-rma/shimless_rma_types.js';
import {disableAllButtons, enableAllButtons} from 'chrome://shimless-rma/shimless_rma_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {eventToPromise, isVisible} from '../test_util.js';

suite('shimlessRMAAppTest', function() {
  /** @type {?ShimlessRma} */
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
    document.body.innerHTML = trustedTypes.emptyHTML;
    service.reset();
  });

  /**
   * @param {!Array<!StateResult>} states
   * @param {string} chromeVersion
   */
  function initializeShimlessRMAApp(states, chromeVersion) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setStates(states);
    service.setGetCurrentOsVersionResult(chromeVersion);
    service.setCheckForOsUpdatesResult('fake version');

    component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * Utility function to assert navigation buttons
   * TODO(joonbug): expand to cover assertion of ButtonState
   */
  function assertNavButtons() {
    const nextButton = component.shadowRoot.querySelector('#next');
    const exitButton = component.shadowRoot.querySelector('#exit');
    const backButton = component.shadowRoot.querySelector('#back');
    assertTrue(!!nextButton);
    assertTrue(!!exitButton);
    assertTrue(!!backButton);
  }

  /**
   * Utility function to click next button
   * @return {Promise}
   */
  function clickNext() {
    // Make sure the Next button is enabled.
    component.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
    const nextButton = component.shadowRoot.querySelector('#next');
    nextButton.click();
    return flushTasks();
  }

  /**
   * Utility function to click back button
   * @return {Promise}
   */
  function clickBack() {
    const backButton = component.shadowRoot.querySelector('#back');
    backButton.click();
    return flushTasks();
  }

  /**
   * Utility function to click exit button
   * @return {Promise}
   */
  function clickExit() {
    const exitButton = component.shadowRoot.querySelector('#exit');
    exitButton.click();
    return flushTasks();
  }

  /**
   * @param {string} buttonNameSelector
   * @return {!Promise}
   */
  function clickButton(buttonNameSelector) {
    assertTrue(!!component);

    const button = component.shadowRoot.querySelector(buttonNameSelector);
    button.click();
    return flushTasks();
  }

  /** @return {!Promise} */
  function openLogsDialog() {
    component.dispatchEvent(new CustomEvent(
        'open-logs-dialog',
        {bubbles: true, composed: true},
        ));
    return flushTasks();
  }

  test('ShimlessRMALoaded', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    assertNavButtons();

    // The Hardware Error page should be hidden by default.
    const hardwareErrorPage =
        component.shadowRoot.querySelector('hardware-error-page');
    assertFalse(!!hardwareErrorPage);
  });

  test('ShimlessRMABasicNavigation', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const prevButton = component.shadowRoot.querySelector('#back');
    const exitButton = component.shadowRoot.querySelector('#exit');
    assertTrue(!!prevButton);
    assertTrue(!!exitButton);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);
    assertFalse(initialPage.hidden);
    assertFalse(initialPage.allButtonsDisabled);
    assertTrue(prevButton.hidden);
    assertTrue(exitButton.hidden);

    // This enables the next button on the landing page.
    service.triggerHardwareVerificationStatusObserver(true, '', 0);
    await flushTasks();
    await clickNext();

    const selectNetworkPage =
        component.shadowRoot.querySelector('onboarding-network-page');
    assertTrue(!!selectNetworkPage);
    assertFalse(selectNetworkPage.hidden);
    assertTrue(!!initialPage);
    assertTrue(initialPage.hidden);
    assertFalse(prevButton.hidden);
    assertFalse(exitButton.hidden);

    prevButton.click();
    await flushTasks();

    // components page should not be destroyed.
    assertTrue(!!selectNetworkPage);
    assertTrue(selectNetworkPage.hidden);
    assertFalse(initialPage.hidden);
    assertTrue(prevButton.hidden);
    assertTrue(exitButton.hidden);
  });

  test('ShimlessRMAExit', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    let abortRmaCount = 0;
    service.abortRma = () => {
      abortRmaCount++;
      return Promise.resolve(RmadErrorCode.kOk);
    };
    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    const exitButton = component.shadowRoot.querySelector('#exit');

    assertFalse(initialPage.allButtonsDisabled);
    exitButton.click();

    const exitDialog = component.shadowRoot.querySelector('#exitDialog');
    assertTrue(exitDialog.open);

    const confirmExitButton =
        component.shadowRoot.querySelector('#confirmExitDialogButton');
    assertTrue(!!confirmExitButton);
    confirmExitButton.click();
    await flushTasks();

    assertEquals(1, abortRmaCount);
    assertTrue(initialPage.allButtonsDisabled);
  });

  test('CancelExitDialog', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    let abortRmaCount = 0;
    service.abortRma = () => {
      abortRmaCount++;
      return Promise.resolve(RmadErrorCode.kOk);
    };
    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    const exitButton = component.shadowRoot.querySelector('#exit');

    assertFalse(initialPage.allButtonsDisabled);
    exitButton.click();
    const exitDialog = component.shadowRoot.querySelector('#exitDialog');
    assertTrue(exitDialog.open);
    component.shadowRoot.querySelector('#cancelExitDialogButton').click();
    assertFalse(exitDialog.open);
    await flushTasks();

    assertEquals(0, abortRmaCount);
    assertFalse(initialPage.allButtonsDisabled);
  });

  test('NextButtonClickedOnReady', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);

    const resolver = new PromiseResolver();
    initialPage.onNextButtonClick = () => resolver.promise;
    assertFalse(initialPage.allButtonsDisabled);

    await clickNext();
    assertFalse(initialPage.hidden);
    assertTrue(initialPage.allButtonsDisabled);

    resolver.resolve(
        {stateResult: {state: State.kUpdateOs, error: RmadErrorCode.kOk}});
    await flushTasks();

    const updatePage =
        component.shadowRoot.querySelector('onboarding-update-page');
    assertTrue(!!updatePage);
    assertFalse(updatePage.hidden);
    assertFalse(updatePage.allButtonsDisabled);
    assertTrue(initialPage.hidden);
  });

  test('NextButtonClickedOnNotReady', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);

    const resolver = new PromiseResolver();
    initialPage.onNextButtonClick = () => resolver.promise;

    await clickNext();
    assertFalse(initialPage.hidden);

    resolver.reject();
    await flushTasks();

    assertFalse(initialPage.hidden);
  });

  test('UpdateButtonState', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const backButton = component.shadowRoot.querySelector('#back');
    assertTrue(!!backButton);
    assertTrue(backButton.hidden);

    component.updateButtonState('buttonBack', ButtonState.VISIBLE);
    await flushTasks();

    assertFalse(backButton.hidden);
  });

  test('UpdateNextButtonLabel', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canExit: true,
          canGoBack: true,
          error: RmadErrorCode.kOk,
        }],
        fakeChromeVersion[0]);

    const nextButton = component.shadowRoot.querySelector('#nextButtonLabel');
    assertEquals(
        loadTimeData.getString('nextButtonLabel'),
        nextButton.textContent.trim());

    component.dispatchEvent(new CustomEvent(
        'set-next-button-label',
        {bubbles: true, composed: true, detail: 'skipButtonLabel'},
        ));
    assertEquals(
        loadTimeData.getString('skipButtonLabel'),
        nextButton.textContent.trim());
  });

  test('NextButtonSpinner', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canExit: true,
          canGoBack: true,
          error: RmadErrorCode.kOk,
        }],
        fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-select-components-page');
    assertTrue(!!initialPage);

    const nextButtonSpinner =
        component.shadowRoot.querySelector('#nextButtonSpinner');
    const backButtonSpinner =
        component.shadowRoot.querySelector('#backButtonSpinner');
    const exitButtonSpinner =
        component.shadowRoot.querySelector('#exitButtonSpinner');

    // Next spinner
    const nextResolver = new PromiseResolver();
    initialPage.onNextButtonClick = () => nextResolver.promise;
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    await clickNext();
    assertFalse(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    nextResolver.resolve({state: State.kUpdateOs, error: RmadErrorCode.kOk});
    await flushTasks();

    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);
  });

  test('BackButtonSpinner', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canExit: true,
          canGoBack: true,
          error: RmadErrorCode.kOk,
        }],
        fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-select-components-page');
    assertTrue(!!initialPage);

    const nextButtonSpinner =
        component.shadowRoot.querySelector('#nextButtonSpinner');
    const backButtonSpinner =
        component.shadowRoot.querySelector('#backButtonSpinner');
    const exitButtonSpinner =
        component.shadowRoot.querySelector('#exitButtonSpinner');

    // Back spinner
    const backResolver = new PromiseResolver();
    service.transitionPreviousState = () => {
      return backResolver.promise;
    };
    await clickBack();
    assertTrue(nextButtonSpinner.hidden);
    assertFalse(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    backResolver.resolve(
        {stateResult: {state: State.kUpdateOs, error: RmadErrorCode.kOk}});
    await flushTasks();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);
  });

  test('ExitButtonSpinner', async () => {
    await initializeShimlessRMAApp(
        [{
          state: State.kSelectComponents,
          canExit: true,
          canGoBack: true,
          error: RmadErrorCode.kOk,
        }],
        fakeChromeVersion[0]);

    const initialPage =
        component.shadowRoot.querySelector('onboarding-select-components-page');
    assertTrue(!!initialPage);

    const nextButtonSpinner =
        component.shadowRoot.querySelector('#nextButtonSpinner');
    const backButtonSpinner =
        component.shadowRoot.querySelector('#backButtonSpinner');
    const exitButtonSpinner =
        component.shadowRoot.querySelector('#exitButtonSpinner');

    // Exit spinner
    const exitResolver = new PromiseResolver();
    service.abortRma = () => {
      return exitResolver.promise;
    };
    await clickExit();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    const exitDialog = component.shadowRoot.querySelector('#exitDialog');
    assertTrue(exitDialog.open);
    await component.shadowRoot.querySelector('#confirmExitDialogButton')
        .click();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertFalse(exitButtonSpinner.hidden);

    exitResolver.resolve({state: State.kUpdateOs, error: RmadErrorCode.kOk});
    await flushTasks();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);
  });

  test('AllButtonsDisabled', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    const nextButton = component.shadowRoot.querySelector('#next');
    const backButton = component.shadowRoot.querySelector('#back');
    const exitButton = component.shadowRoot.querySelector('#exit');
    const busyStateOverlay = /** @type {!HTMLElement} */ (
        component.shadowRoot.querySelector('#busyStateOverlay'));

    assertFalse(nextButton.disabled);
    assertFalse(backButton.disabled);
    assertFalse(exitButton.disabled);

    disableAllButtons(component, /*showBusyStateOverlay=*/ false);
    assertTrue(nextButton.disabled);
    assertTrue(backButton.disabled);
    assertTrue(exitButton.disabled);
    assertFalse(isVisible(busyStateOverlay));

    disableAllButtons(component, /*showBusyStateOverlay=*/ true);
    assertTrue(isVisible(busyStateOverlay));

    enableAllButtons(component);
    assertFalse(nextButton.disabled);
    assertFalse(backButton.disabled);
    assertFalse(exitButton.disabled);
    assertFalse(isVisible(busyStateOverlay));
  });

  test('ExitButtonClickEventIsHandled', async () => {
    const resolver = new PromiseResolver();

    await initializeShimlessRMAApp(
        [{
          state: State.kWelcomeScreen,
          canExit: true,
          canGoBack: true,
          error: RmadErrorCode.kOk,
        }],
        fakeChromeVersion[0]);

    let callCounter = 0;
    service.abortRma = () => {
      callCounter++;
      return resolver.promise;
    };

    component.dispatchEvent(new CustomEvent(
        'click-exit-button',
        {bubbles: true, composed: true},
        ));

    await flushTasks();
    const exitDialog = component.shadowRoot.querySelector('#exitDialog');
    assertTrue(exitDialog.open);
    assertEquals(0, callCounter);
  });

  test('TransitionStateListener', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    // Confirm starting on the landing page.
    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);

    // Attempt to transition OS Update page.
    component.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: () => Promise.resolve({
            stateResult: {state: State.kUpdateOs, error: RmadErrorCode.kOk},
          }),
        },
        ));
    await flushTasks();

    // Confirm transition to the OS Update page.
    const updatePage =
        component.shadowRoot.querySelector('onboarding-update-page');
    assertTrue(!!updatePage);
  });

  test('StateResultCanExitCanGoBack', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    // Confirm starting on the landing page.
    const initialPage =
        component.shadowRoot.querySelector('onboarding-landing-page');
    assertTrue(!!initialPage);

    component.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: () => Promise.resolve({
            stateResult: {
              state: State.kUpdateOs,
              error: RmadErrorCode.kOk,
              canExit: false,
              canGoBack: false,
            },
          }),
        },
        ));
    await flushTasks();

    // Confirm transition to the OS Update page.
    const updatePage =
        component.shadowRoot.querySelector('onboarding-update-page');
    assertTrue(!!updatePage);
    const backButton = component.shadowRoot.querySelector('#back');
    const exitButton = component.shadowRoot.querySelector('#exit');

    // Both buttons should be hidden due to the `stateResult` response.
    assertTrue(backButton.hidden);
    assertTrue(exitButton.hidden);

    // Initialize the fake data.
    service.setGetCalibrationComponentListResult(
        fakeCalibrationComponentsWithFails);

    // Attempt to transition to Calibration Failed page.
    component.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: () => Promise.resolve({
            stateResult: {
              state: State.kCheckCalibration,
              error: RmadErrorCode.kOk,
              canExit: false,
              canGoBack: false,
            },
          }),
        },
        ));
    await flushTasks();

    // The exit button should never be hidden for the Calibration failed page.
    assertTrue(backButton.hidden);
    assertFalse(exitButton.hidden);
  });

  test('HardwareErrorEventIsHandled', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    component.dispatchEvent(new CustomEvent(
        'fatal-hardware-error',
        {
          bubbles: true,
          composed: true,
          detail: {
            rmadErrorCode: RmadErrorCode.kProvisioningFailed,
            fatalErrorCode: 1001,
          },
        },
        ));

    await flushTasks();

    // Confirm transition to the Hardware Error page.
    const hardwareErrorPage =
        component.shadowRoot.querySelector('hardware-error-page');
    assertTrue(!!hardwareErrorPage);
  });

  test('RebootErrorCodeResultsInShowingRebootPage', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    // Emulate platform sending a reboot error code.
    component.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: () => Promise.resolve({
            stateResult: {
              state: State.kWPDisableComplete,
              error: RmadErrorCode.kExpectReboot,
            },
          }),
        },
        ));
    await flushTasks();

    // Confirm transition to the reboot page.
    const rebootPage = component.shadowRoot.querySelector('reboot-page');
    assertTrue(!!rebootPage);
  });

  test('ShutdownErrorCodeResultsInShowingShutdownPage', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    // Emulate platform sending a shut down error code.
    component.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: () => Promise.resolve({
            stateResult: {
              state: State.kWPDisableComplete,
              error: RmadErrorCode.kExpectShutdown,
            },
          }),
        },
        ));
    await flushTasks();

    // Confirm transition to the reboot page.
    const rebootPage = component.shadowRoot.querySelector('reboot-page');
    assertTrue(!!rebootPage);
  });

  test('SaveLogsToUsb', async () => {
    const resolver = new PromiseResolver();
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    service.triggerExternalDiskObserver(true, 0);
    await flushTasks();

    let callCount = 0;
    service.saveLog = () => {
      callCount++;
      return resolver.promise;
    };

    await openLogsDialog();
    assertTrue(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));

    // Attempt to save the logs.
    await clickButton('#saveLogDialogButton');
    const savePath = 'save/path';
    resolver.resolve({savePath: {path: savePath}, error: RmadErrorCode.kOk});
    await flushTasks();

    assertEquals(1, callCount);

    // The save log button should be replaced by the done button.
    assertFalse(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));
    assertTrue(isVisible(
        component.shadowRoot.querySelector('#logSaveDoneDialogButton')));
    assertEquals(
        loadTimeData.getStringF('rmaLogsSaveSuccessText', savePath),
        component.shadowRoot.querySelector('#logSavedStatusText')
            .textContent.trim());

    // Close the logs dialog.
    await clickButton('#logSaveDoneDialogButton');
    await flushTasks();

    // Open the logs dialog and verify we are at the original state with the
    // Save Log button displayed.
    await openLogsDialog();
    assertTrue(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));
  });

  test('SaveLogFails', async () => {
    const resolver = new PromiseResolver();
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    service.triggerExternalDiskObserver(true, 0);
    await flushTasks();

    let callCount = 0;
    service.saveLog = () => {
      callCount++;
      return resolver.promise;
    };

    await openLogsDialog();
    assertTrue(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));

    // Attempt to save the logs but it fails.
    await clickButton('#saveLogDialogButton');
    resolver.resolve(
        {savePath: 'save/path', error: RmadErrorCode.kCannotSaveLog});
    await flushTasks();

    assertEquals(1, callCount);

    // The save log button should be replaced by the done button and the retry
    // button.
    assertFalse(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));
    assertTrue(isVisible(
        component.shadowRoot.querySelector('#logSaveDoneDialogButton')));
    assertTrue(
        isVisible(component.shadowRoot.querySelector('#logRetryDialogButton')));
    assertEquals(
        loadTimeData.getString('rmaLogsSaveFailText'),
        component.shadowRoot.querySelector('#logSavedStatusText')
            .textContent.trim());

    // Click the retry button and verify that it retries saving the logs.
    await clickButton('#logRetryDialogButton');
    resolver.resolve(
        {savePath: 'save/path', error: RmadErrorCode.kCannotSaveLog});
    await flushTasks();

    assertEquals(2, callCount);
  });

  test('SaveLogFailsUsbNotFound', async () => {
    const resolver = new PromiseResolver();
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    service.triggerExternalDiskObserver(true, 0);
    await flushTasks();

    let callCount = 0;
    service.saveLog = () => {
      callCount++;
      return resolver.promise;
    };

    await openLogsDialog();
    assertTrue(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));

    // Attempt to save the logs but it fails because the USB is not detected.
    await clickButton('#saveLogDialogButton');
    resolver.resolve(
        {savePath: 'save/path', error: RmadErrorCode.kUsbNotFound});
    await flushTasks();

    assertEquals(1, callCount);

    // The save log button should be replaced by the done button and the retry
    // button.
    assertFalse(
        isVisible(component.shadowRoot.querySelector('#saveLogDialogButton')));
    assertTrue(isVisible(
        component.shadowRoot.querySelector('#logSaveDoneDialogButton')));
    assertTrue(
        isVisible(component.shadowRoot.querySelector('#logRetryDialogButton')));
    assertEquals(
        loadTimeData.getString('rmaLogsSaveUsbNotFound'),
        component.shadowRoot.querySelector('#logSavedStatusText')
            .textContent.trim());
  });

  test('ExternalDiskConnectedShowsUsbActionButtons', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    service.triggerExternalDiskObserver(true, 0);
    await flushTasks();

    const logConnectUsbMessageContainer =
        component.shadowRoot.querySelector('#logConnectUsbMessageContainer');
    assertTrue(!!logConnectUsbMessageContainer);
    assertTrue(logConnectUsbMessageContainer.hidden);

    const saveLogButtonContainer =
        component.shadowRoot.querySelector('#saveLogButtonContainer');
    assertTrue(!!saveLogButtonContainer);
    assertFalse(saveLogButtonContainer.hidden);

    const logSaveAttemptButtonContainer =
        component.shadowRoot.querySelector('#logSaveAttemptButtonContainer');
    assertTrue(!!logSaveAttemptButtonContainer);
    assertTrue(logSaveAttemptButtonContainer.hidden);
  });

  test('ExternalDiskDisconnectedShowsMissingUsbMessage', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    service.triggerExternalDiskObserver(false, 0);
    await flushTasks();

    const logConnectUsbMessageContainer =
        component.shadowRoot.querySelector('#logConnectUsbMessageContainer');
    assertTrue(!!logConnectUsbMessageContainer);
    assertFalse(logConnectUsbMessageContainer.hidden);

    const saveLogButtonContainer =
        component.shadowRoot.querySelector('#saveLogButtonContainer');
    assertTrue(!!saveLogButtonContainer);
    assertTrue(saveLogButtonContainer.hidden);

    const logSaveAttemptButtonContainer =
        component.shadowRoot.querySelector('#logSaveAttemptButtonContainer');
    assertTrue(!!logSaveAttemptButtonContainer);
    assertTrue(logSaveAttemptButtonContainer.hidden);
  });

  test('LogsDialogCloses', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);
    await openLogsDialog();

    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(logsDialog.open);

    await clickButton('#closeLogDialogButton');

    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);

    // Verify logs dialog can be closed when the USB is unplugged.
    service.triggerExternalDiskObserver(false, 0);
    await openLogsDialog();
    assertTrue(logsDialog.open);

    await clickButton('#closeLogDialogButton');

    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);
  });

  test('KeyboardShortcutOpensLogsDialog', async () => {
    await initializeShimlessRMAApp(fakeStates, fakeChromeVersion[0]);

    // Confirm logs dialog starts closed.
    const logsDialog = component.shadowRoot.querySelector('#logsDialog');
    assertTrue(!!logsDialog);
    assertFalse(logsDialog.open);

    const keydownEventPromise = eventToPromise('keydown', component);
    component.dispatchEvent(new KeyboardEvent(
        'keydown',
        {
          bubbles: true,
          composed: true,
          key: 'L',
          altKey: true,
          shiftKey: true,
        },
        ));

    await keydownEventPromise;
    assertTrue(logsDialog.open);
  });
});
