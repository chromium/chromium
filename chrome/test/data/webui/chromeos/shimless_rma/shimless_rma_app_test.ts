// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {CLICK_EXIT_BUTTON, DISABLE_NEXT_BUTTON, FATAL_HARDWARE_ERROR, OPEN_LOGS_DIALOG, SET_NEXT_BUTTON_LABEL, TRANSITION_STATE} from 'chrome://shimless-rma/events.js';
import {fakeCalibrationComponentsWithFails, fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {OnboardingSelectComponentsPageElement} from 'chrome://shimless-rma/onboarding_select_components_page.js';
import {ButtonState, ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {RmadErrorCode, State, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {disableAllButtons, enableAllButtons} from 'chrome://shimless-rma/shimless_rma_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('shimlessRMAAppTest', function() {
  let component: ShimlessRma|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  const nextButtonSelector = '#next';
  const backButtonSelector = '#back';
  const exitButtonSelector = '#exit';

  const exitDialogSelector = '#exitDialog';
  const confirmExitButtonSelector = '#confirmExitDialogButton';
  const cancelExitButtonSelector = '#cancelExitDialogButton';

  const saveLogButtonSelector = '#saveLogDialogButton';
  const logSaveDoneButtonSelector = '#logSaveDoneDialogButton';
  const logRetryButtonSelector = '#logRetryDialogButton';
  const logSavedStatusSelector = '#logSavedStatusText';
  const closeLogsDialogButtonSelector = '#closeLogDialogButton';

  const logsDialogSelector = '#logsDialog';
  const logConnectUsbMessageSelector = '#logConnectUsbMessageContainer';
  const saveLogContainerSelector = '#saveLogButtonContainer';

  const hardwareErrorPageSelector = 'hardware-error-page';
  const landingPageSelector = 'onboarding-landing-page';
  const networkPageSelector = 'onboarding-network-page';
  const updatePageSelector = 'onboarding-update-page';
  const rebootPageSelector = 'reboot-page';
  const selectComponentsPageSelector = 'onboarding-select-components-page';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  function initializeShimlessRMAApp(states: StateResult[] = fakeStates):
      Promise<void> {
    // Initialize the fake data.
    service.setStates(states);
    service.setGetCurrentOsVersionResult(/* version= */ '');
    service.setCheckForOsUpdatesResult(/* version= */ '');

    assert(!component);
    component = document.createElement(ShimlessRma.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Utility function to click next button
  function clickNext(): Promise<void> {
    assert(component);
    // Make sure the Next button is enabled.
    component.dispatchEvent(new CustomEvent(
        DISABLE_NEXT_BUTTON,
        {bubbles: true, composed: true, detail: false},
        ));
    strictQuery(nextButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  // Utility function to click back button
  function clickBack(): Promise<void> {
    assert(component);
    strictQuery(backButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  // Utility function to click exit button
  function clickExit(): Promise<void> {
    assert(component);
    strictQuery(exitButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  function clickButton(buttonNameSelector: string): Promise<void> {
    assert(component);
    strictQuery(buttonNameSelector, component.shadowRoot, CrButtonElement)
        .click();
    return flushTasks();
  }

  function openLogsDialog(): Promise<void> {
    assert(component);
    component.dispatchEvent(new CustomEvent(
        OPEN_LOGS_DIALOG,
        {bubbles: true, composed: true},
        ));
    return flushTasks();
  }

  // Verify the correct components are loaded
  test('ShimlessRMALoaded', async () => {
    await initializeShimlessRMAApp();

    assert(component);
    assert(
        strictQuery(nextButtonSelector, component.shadowRoot, CrButtonElement));
    assert(
        strictQuery(exitButtonSelector, component.shadowRoot, CrButtonElement));
    assert(
        strictQuery(backButtonSelector, component.shadowRoot, CrButtonElement));

    // The Hardware Error page should be hidden by default.
    assert(!(component.shadowRoot!.querySelector(hardwareErrorPageSelector)));
    // 3P Diagnostics should be loaded.
    strictQuery('#shimless3pDiagnostics', component.shadowRoot, HTMLElement);
  });

  // Verify clicking the next button goes to the next page and the back button
  // goes to the previous page.
  test('ShimlessRMABasicNavigation', async () => {
    await initializeShimlessRMAApp();

    assert(component);
    const initialPage =
        strictQuery(landingPageSelector, component.shadowRoot, HTMLElement);
    assertFalse(initialPage.hidden);

    // This enables the next button on the landing page.
    assert(service);
    service.triggerHardwareVerificationStatusObserver(
        /* isCompliant= */ true, /* errorMessage= */ '', /* delayMs= */ 0);
    await flushTasks();
    await clickNext();

    const selectNetworkPage =
        strictQuery(networkPageSelector, component.shadowRoot, HTMLElement);
    assertFalse(selectNetworkPage.hidden);

    // Click the back button and expect the networking page to be hidden but not
    // destroyed.
    await clickBack();
    assert(selectNetworkPage);
    assertTrue(selectNetworkPage.hidden);
    assertFalse(initialPage.hidden);
  });

  // Verify clicking the exit button attempts to abort Shimless RMA.
  test('ShimlessRMAExit', async () => {
    await initializeShimlessRMAApp();
    let abortRmaCount = 0;
    service.abortRma = () => {
      ++abortRmaCount;
      return Promise.resolve({error: RmadErrorCode.kOk});
    };

    await clickExit();

    assert(component);
    const exitDialog =
        strictQuery(exitDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(exitDialog.open);
    strictQuery(
        confirmExitButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    assertFalse(exitDialog.open);
    assertEquals(1, abortRmaCount);
  });

  // Verify clicking the cancel exit button doesn't attempt to abort Shimless
  // RMA.
  test('CancelExitDialog', async () => {
    await initializeShimlessRMAApp();
    let abortRmaCount = 0;
    service.abortRma = () => {
      ++abortRmaCount;
      return Promise.resolve({error: RmadErrorCode.kOk});
    };

    await clickExit();

    assert(component);
    const exitDialog =
        strictQuery(exitDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(exitDialog.open);
    strictQuery(cancelExitButtonSelector, component.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    assertFalse(exitDialog.open);
    assertEquals(0, abortRmaCount);
  });

  // Verify the page change doesn't change when the next button click is
  // rejected.
  test('NextButtonClickedOnNotReady', async () => {
    await initializeShimlessRMAApp();

    // Confirm the initial page is visible.
    assert(component);
    const initialPage = strictQuery(
        landingPageSelector, component.shadowRoot, OnboardingLandingPage);
    assertFalse(initialPage.hidden);

    // Click the next button expecting to go to the next page.
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    initialPage.onNextButtonClick = () => resolver.promise;
    await clickNext();

    // Reject the next button click and confirm the initial page is still
    // visible.
    resolver.reject();
    await flushTasks();
    assertFalse(initialPage.hidden);
  });

  // Verify the back button becomes visible when protected.
  test('UpdateBackButtonVisibility', async () => {
    await initializeShimlessRMAApp();

    assert(component);
    const backButton =
        strictQuery(backButtonSelector, component.shadowRoot, CrButtonElement);
    assertTrue(backButton.hidden);

    component.updateButtonState(
        /* buttonName= */ 'buttonBack', ButtonState.VISIBLE);
    await flushTasks();

    assertFalse(backButton.hidden);
  });

  // Verify the next button label updates to "Skip" when requested.
  test('UpdateNextButtonLabel', async () => {
    await initializeShimlessRMAApp([{
      state: State.kSelectComponents,
      canExit: true,
      canGoBack: true,
      error: RmadErrorCode.kOk,
    }]);

    assert(component);
    const nextButtonLabel =
        strictQuery('#nextButtonLabel', component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('nextButtonLabel'),
        nextButtonLabel.textContent!.trim());

    // Trigger the next button to update its label.
    component.dispatchEvent(new CustomEvent(
        SET_NEXT_BUTTON_LABEL,
        {bubbles: true, composed: true, detail: 'skipButtonLabel'},
        ));
    assertEquals(
        loadTimeData.getString('skipButtonLabel'),
        nextButtonLabel.textContent!.trim());
  });

  // Verify the correct button spinners are showing based on the current state.
  test('ButtonSpinnerStates', async () => {
    await initializeShimlessRMAApp([{
      state: State.kSelectComponents,
      canExit: true,
      canGoBack: true,
      error: RmadErrorCode.kOk,
    }]);

    assert(component);
    const nextButtonSpinner =
        strictQuery('#nextButtonSpinner', component.shadowRoot, HTMLElement);
    const backButtonSpinner =
        strictQuery('#backButtonSpinner', component.shadowRoot, HTMLElement);
    const exitButtonSpinner =
        strictQuery('#exitButtonSpinner', component.shadowRoot, HTMLElement);
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    // Next spinner
    const nextResolver = new PromiseResolver<{stateResult: StateResult}>();
    strictQuery(
        selectComponentsPageSelector, component.shadowRoot,
        OnboardingSelectComponentsPageElement)
        .onNextButtonClick = () => nextResolver.promise;

    await clickNext();
    assertFalse(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    nextResolver.resolve({
      stateResult: {
        state: State.kUpdateOs,
        error: RmadErrorCode.kOk,
        canExit: false,
        canGoBack: false,
      },
    });
    await flushTasks();

    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    // Back spinner
    const backResolver = new PromiseResolver<{stateResult: StateResult}>();
    assert(service);
    service.transitionPreviousState = () => {
      return backResolver.promise;
    };

    await clickBack();
    assertTrue(nextButtonSpinner.hidden);
    assertFalse(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    backResolver.resolve({
      stateResult: {
        state: State.kUpdateOs,
        error: RmadErrorCode.kOk,
        canExit: false,
        canGoBack: false,
      },
    });
    await flushTasks();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    // Exit spinner
    const exitResolver = new PromiseResolver<{error: RmadErrorCode}>();
    service.abortRma = () => {
      return exitResolver.promise;
    };
    await clickExit();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);

    assert(component);
    await clickButton(confirmExitButtonSelector);
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertFalse(exitButtonSpinner.hidden);

    exitResolver.resolve({error: RmadErrorCode.kOk});
    await flushTasks();
    assertTrue(nextButtonSpinner.hidden);
    assertTrue(backButtonSpinner.hidden);
    assertTrue(exitButtonSpinner.hidden);
  });

  // Verify when requested, all buttons are disabled with busy state overlay
  // visible.
  test('AllButtonsDisabled', async () => {
    await initializeShimlessRMAApp();

    assert(component);
    const nextButton =
        strictQuery(nextButtonSelector, component.shadowRoot, CrButtonElement);
    const backButton =
        strictQuery(nextButtonSelector, component.shadowRoot, CrButtonElement);
    const exitButton =
        strictQuery(exitButtonSelector, component.shadowRoot, CrButtonElement);
    const busyStateOverlay =
        strictQuery('#busyStateOverlay', component.shadowRoot, HTMLElement);

    assertFalse(nextButton.disabled);
    assertFalse(backButton.disabled);
    assertFalse(exitButton.disabled);

    disableAllButtons(component, /* showBusyStateOverlay= */ false);
    assertTrue(nextButton.disabled);
    assertTrue(backButton.disabled);
    assertTrue(exitButton.disabled);
    assertFalse(isVisible(busyStateOverlay));

    disableAllButtons(component, /* showBusyStateOverlay= */ true);
    assertTrue(isVisible(busyStateOverlay));

    enableAllButtons(component);
    assertFalse(nextButton.disabled);
    assertFalse(backButton.disabled);
    assertFalse(exitButton.disabled);
    assertFalse(isVisible(busyStateOverlay));
  });

  // Verify the exit button event opens the exit dialog.
  test('ExitButtonClickEventIsHandled', async () => {
    await initializeShimlessRMAApp();

    let callCounter = 0;
    const resolver = new PromiseResolver<{error: RmadErrorCode}>();
    assert(service);
    service.abortRma = () => {
      ++callCounter;
      return resolver.promise;
    };

    assert(component);
    component.dispatchEvent(new CustomEvent(
        CLICK_EXIT_BUTTON,
        {bubbles: true, composed: true},
        ));

    await flushTasks();
    assertTrue(
        strictQuery(exitDialogSelector, component.shadowRoot, CrDialogElement)
            .open);
    assertEquals(0, callCounter);
  });

  // Verify the "transition state" event is transitions to the requested page.
  test('TransitionStateListener', async () => {
    await initializeShimlessRMAApp();

    // Attempt to transition OS Update page.
    assert(component);
    component.dispatchEvent(new CustomEvent(
        TRANSITION_STATE,
        {
          bubbles: true,
          composed: true,
          detail: () => Promise.resolve({
            stateResult: {state: State.kUpdateOs, error: RmadErrorCode.kOk},
            canExit: false,
            canGoBack: false,
          }),
        },
        ));
    await flushTasks();

    // Confirm transition to the OS Update page.
    assert(strictQuery(updatePageSelector, component.shadowRoot, HTMLElement));

    // Both buttons should be hidden due to the `stateResult` response.
    assertTrue(
        strictQuery(backButtonSelector, component.shadowRoot, CrButtonElement)
            .hidden);
    assertTrue(
        strictQuery(exitButtonSelector, component.shadowRoot, CrButtonElement)
            .hidden);
  });

  // Verify the back button can't be hidden on the failed calibartion page.
  test('FailedCalibrationHiddenExitButton', async () => {
    await initializeShimlessRMAApp();

    // Initialize the fake data.
    assert(service);
    service.setGetCalibrationComponentListResult(
        fakeCalibrationComponentsWithFails);

    // Transition to Calibration Failed page but attempt to hide the back button
    // by setting `canGoBack` to false.
    assert(component);
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

    // The back button will be hidden but the exit button should never be hidden
    // for the Calibration failed page.
    assertTrue(
        strictQuery(backButtonSelector, component.shadowRoot, CrButtonElement)
            .hidden);
    assertFalse(
        strictQuery(exitButtonSelector, component.shadowRoot, CrButtonElement)
            .hidden);
  });

  // Verify the "fatal hardware error" event launches the hardware error page.
  test('HardwareErrorEventIsHandled', async () => {
    await initializeShimlessRMAApp();

    assert(component);
    component.dispatchEvent(new CustomEvent(
        FATAL_HARDWARE_ERROR,
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
    assert(strictQuery(
        hardwareErrorPageSelector, component.shadowRoot, HTMLElement));
  });

  // Verify the getting the `kExpectReboot` code transitions to the reboot page.
  test('RebootErrorCodeResultsInShowingRebootPage', async () => {
    await initializeShimlessRMAApp();

    // Emulate platform sending a reboot error code.
    assert(component);
    component.dispatchEvent(new CustomEvent(
        TRANSITION_STATE,
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
    assert(strictQuery(rebootPageSelector, component.shadowRoot, HTMLElement));
  });

  // Verify the getting the `kExpectShutdown` code transitions to the reboot
  // page.
  test('ShutdownErrorCodeResultsInRebootPage', async () => {
    await initializeShimlessRMAApp();

    // Emulate platform sending a shut down error code.
    assert(component);
    component.dispatchEvent(new CustomEvent(
        TRANSITION_STATE,
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
    assert(strictQuery(rebootPageSelector, component.shadowRoot, HTMLElement));
  });

  // Verify logs can be saved via the save logs dialog.
  test('SaveLogsToUsb', async () => {
    await initializeShimlessRMAApp();

    assert(service);
    service.triggerExternalDiskObserver(/* detected= */ true, /* delayMs= */ 0);
    await flushTasks();

    let saveLogCallCount = 0;
    const resolver =
        new PromiseResolver<{savePath: FilePath, error: RmadErrorCode}>();
    service.saveLog = () => {
      ++saveLogCallCount;
      return resolver.promise;
    };

    await openLogsDialog();

    // Attempt to save the logs.
    await clickButton(saveLogButtonSelector);
    const savePath = 'save/path';
    resolver.resolve({savePath: {path: savePath}, error: RmadErrorCode.kOk});
    await flushTasks();

    assertEquals(1, saveLogCallCount);

    // The save log button should be replaced by the done button.
    assert(component);
    assertFalse(isVisible(strictQuery(
        saveLogButtonSelector, component.shadowRoot, CrButtonElement)));
    assertTrue(isVisible(strictQuery(
        logSaveDoneButtonSelector, component.shadowRoot, CrButtonElement)));
    assertEquals(
        loadTimeData.getStringF('rmaLogsSaveSuccessText', savePath),
        strictQuery(logSavedStatusSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());

    // Close the logs dialog.
    await clickButton(logSaveDoneButtonSelector);
    await flushTasks();

    // Open the logs dialog and verify we are at the original state with the
    // Save Log button displayed.
    await openLogsDialog();
    assertTrue(isVisible(strictQuery(
        saveLogButtonSelector, component.shadowRoot, CrButtonElement)));
  });

  // Verify the save logs dialog shows error messasges for a failed save and
  // allows for a retry.
  test('SaveLogFails', async () => {
    await initializeShimlessRMAApp();

    assert(service);
    service.triggerExternalDiskObserver(/* detected= */ true, /* delayMs= */ 0);
    await flushTasks();

    let saveLogCallCount = 0;
    const resolver =
        new PromiseResolver<{savePath: FilePath, error: RmadErrorCode}>();
    service.saveLog = () => {
      ++saveLogCallCount;
      return resolver.promise;
    };

    // Attempt to save the logs but it fails.
    await openLogsDialog();
    await clickButton(saveLogButtonSelector);
    resolver.resolve(
        {savePath: {'path': 'save/path'}, error: RmadErrorCode.kCannotSaveLog});
    await flushTasks();

    assertEquals(1, saveLogCallCount);

    // The save log button should be replaced by the done button and the retry
    // button.
    assert(component);
    assertFalse(isVisible(strictQuery(
        saveLogButtonSelector, component.shadowRoot, CrButtonElement)));
    assertTrue(isVisible(strictQuery(
        logRetryButtonSelector, component.shadowRoot, CrButtonElement)));
    assertEquals(
        loadTimeData.getString('rmaLogsSaveFailText'),
        strictQuery(logSavedStatusSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());

    // Click the retry button and verify that it retries saving the logs.
    await clickButton(logRetryButtonSelector);
    resolver.resolve(
        {savePath: {'path': 'save/path'}, error: RmadErrorCode.kCannotSaveLog});
    await flushTasks();

    assertEquals(2, saveLogCallCount);
  });

  // Verify the correct message is shown for USB not found when saving logs.
  test('SaveLogFailsUsbNotFound', async () => {
    await initializeShimlessRMAApp();

    assert(service);
    service.triggerExternalDiskObserver(/* detected= */ true, /* delayMs= */ 0);
    await flushTasks();

    const resolver =
        new PromiseResolver<{savePath: FilePath, error: RmadErrorCode}>();
    service.saveLog = () => {
      return resolver.promise;
    };

    // Attempt to save the logs but it fails because the USB is not detected.
    await openLogsDialog();
    await clickButton(saveLogButtonSelector);
    resolver.resolve(
        {savePath: {path: 'save/path'}, error: RmadErrorCode.kUsbNotFound});
    await flushTasks();

    // The save log button should be replaced by the done button and the retry
    // button.
    assert(component);
    assertFalse(isVisible(strictQuery(
        saveLogButtonSelector, component.shadowRoot, CrButtonElement)));
    // await waitAfterNextRender(strictQuery(logRetryButtonSelector,
    // component.shadowRoot, CrButtonElement)); await flushTasks();
    assertTrue(isVisible(strictQuery(
        logRetryButtonSelector, component.shadowRoot, CrButtonElement)));
    assertEquals(
        loadTimeData.getString('rmaLogsSaveUsbNotFound'),
        strictQuery(logSavedStatusSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());
  });

  // Verify the correct message is shown for USB connected or disconnected.
  test('ExternalDiskConnectedShowsUsbActionButtons', async () => {
    await initializeShimlessRMAApp();

    assert(component);
    const logConnectUsbMessageContainer = strictQuery(
        logConnectUsbMessageSelector, component.shadowRoot, HTMLElement);
    const saveLogButtonContainer = strictQuery(
        saveLogContainerSelector, component.shadowRoot, HTMLElement);

    // When an external disk is connected, verify the "save log" message is
    // displayed.
    assert(service);
    service.triggerExternalDiskObserver(/* detected= */ true, /* delayMs= */ 0);
    await flushTasks();
    assertTrue(logConnectUsbMessageContainer.hidden);
    assertFalse(saveLogButtonContainer.hidden);

    // When an eexternal disk isn't connected, verify the "connect a USB"
    // message is displayed.
    service.triggerExternalDiskObserver(
        /* detected= */ false, /* delayMs= */ 0);
    await flushTasks();
    assertFalse(logConnectUsbMessageContainer.hidden);
    assertTrue(saveLogButtonContainer.hidden);
  });

  // Verify the save logs dialog can be closed even after the USB is unplugged.
  test('LogsDialogCloses', async () => {
    await initializeShimlessRMAApp();

    await openLogsDialog();
    assert(component);
    const logsDialog =
        strictQuery(logsDialogSelector, component.shadowRoot, CrDialogElement);
    assertTrue(logsDialog.open);

    await clickButton(closeLogsDialogButtonSelector);
    assertFalse(logsDialog.open);

    // Verify logs dialog can be closed when the USB is unplugged.
    service.triggerExternalDiskObserver(false, 0);
    await openLogsDialog();
    assertTrue(logsDialog.open);

    await clickButton(closeLogsDialogButtonSelector);
    assertFalse(logsDialog.open);
  });

  // Verify the Alt+Shift+L keyboard shortcut opens the logs dialog.
  test('KeyboardShortcutOpensLogsDialog', async () => {
    await initializeShimlessRMAApp();

    // Confirm logs dialog starts closed.
    assert(component);
    const logsDialog =
        strictQuery(logsDialogSelector, component.shadowRoot, CrDialogElement);
    assertFalse(logsDialog.open);

    // Invoke the Shimless logs keyboard shortcut.
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
