// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';

// #import {PSimUIState, PSimPageName, PSimSetupFlowResult, PSIM_SETUP_RESULT_METRIC_NAME, SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME, FAILED_PSIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';
// #import {setCellularSetupRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {ButtonState} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// #import {FakeCarrierPortalHandlerRemote, FakeCellularSetupRemote} from './fake_cellular_setup_remote.m.js';
// #import {MockMetricsPrivate} from './mock_metrics_private.m.js';
// clang-format on

suite('CrComponentsPsimFlowUiTest', function() {
  let pSimPage;

  /** @type {?chromeos.cellularSetup.mojom.CellularSetupRemote} */
  let cellularSetupRemote = null;

  /** @type {?FakeCarrierPortalHandlerRemote} */
  let cellularCarrierHandler = null;

  /** @type {?chromeos.cellularSetup.mojom.ActivationDelegateReceiver} */
  let cellularActivationDelegate = null;

  /** @type {function(Function, number)} */
  let timeoutFunction = null;

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /** @param {PSimSetupFlowResult} pSimSetupFlowResult */
  function endFlowAndVerifyResult(pSimSetupFlowResult) {
    pSimPage.remove();
    Polymer.dom.flush();
    assertEquals(
        chrome.metricsPrivate.getHistogramEnumValueCount(pSimSetupFlowResult),
        1);

    if (pSimSetupFlowResult === PSimSetupFlowResult.SUCCESS) {
      assertEquals(
          chrome.metricsPrivate.getHistogramCount(
              FAILED_PSIM_SETUP_DURATION_METRIC_NAME),
          0);
      assertEquals(
          chrome.metricsPrivate.getHistogramCount(
              SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME),
          1);
      return;
    }

    assertEquals(
        chrome.metricsPrivate.getHistogramCount(
            FAILED_PSIM_SETUP_DURATION_METRIC_NAME),
        1);
    assertEquals(
        chrome.metricsPrivate.getHistogramCount(
            SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME),
        0);
  }

  setup(function() {
    cellularCarrierHandler =
        new cellular_setup.FakeCarrierPortalHandlerRemote();
    cellularSetupRemote =
        new cellular_setup.FakeCellularSetupRemote(cellularCarrierHandler);
    cellular_setup.setCellularSetupRemoteForTesting(cellularSetupRemote);
    chrome.metricsPrivate = new MockMetricsPrivate();

    pSimPage = document.createElement('psim-flow-ui');
    pSimPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    pSimPage.setTimerFunctionForTest(function(fn, milliseconds) {
      timeoutFunction = fn;
      return 1;
    });
    pSimPage.initSubflow();
    document.body.appendChild(pSimPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    pSimPage.remove();
  });

  test('Show provisioning page on activation finished', async () => {
    cellularActivationDelegate =
        cellularSetupRemote.getLastActivationDelegate();

    let provisioningPage = pSimPage.$$('#provisioningPage');
    assertTrue(!!provisioningPage);
    assertFalse(
        pSimPage.selectedPSimPageName_ ===
        cellularSetup.PSimPageName.provisioningPage);

    cellularActivationDelegate.onActivationFinished(
        chromeos.cellularSetup.mojom.ActivationResult
            .kSuccessfullyStartedActivation);

    await flushAsync();

    assertTrue(
        pSimPage.selectedPSimPageName_ ===
        cellularSetup.PSimPageName.PROVISIONING);

    endFlowAndVerifyResult(PSimSetupFlowResult.SUCCESS);
  });

  test('Sim detection failure with retries', async function() {
    assertTrue(
        pSimPage.state_ === cellularSetup.PSimUIState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.currentTimeoutId_);

    await flushAsync();

    // Simulate timeout.
    timeoutFunction();

    assertTrue(
        pSimPage.state_ === cellularSetup.PSimUIState.TIMEOUT_START_ACTIVATION);
    assertTrue(pSimPage.forwardButtonLabel === 'Try again');

    // Simulate clicking 'Try Again'.
    pSimPage.navigateForward();

    assertTrue(
        pSimPage.state_ === cellularSetup.PSimUIState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.currentTimeoutId_);

    await flushAsync();

    // Timeout again.
    timeoutFunction();

    assertTrue(
        pSimPage.state_ === cellularSetup.PSimUIState.TIMEOUT_START_ACTIVATION);
    assertTrue(pSimPage.forwardButtonLabel === 'Try again');

    // Click 'Try Again' again.
    pSimPage.navigateForward();

    assertTrue(
        pSimPage.state_ === cellularSetup.PSimUIState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.currentTimeoutId_);

    await flushAsync();

    // Timeout again.
    timeoutFunction();

    // Should now be at the failure state.
    assertTrue(
        pSimPage.state_ ===
        cellularSetup.PSimUIState.FINAL_TIMEOUT_START_ACTIVATION);
  });

  test('Carrier title on provisioning page', async () => {
    cellularActivationDelegate =
        cellularSetupRemote.getLastActivationDelegate();

    cellularActivationDelegate.onActivationStarted({
      paymentUrl: {url: ''},
      paymentPostData: 'verison_post_data',
      carrier: 'Verizon wireless',
      meid: '012345678912345',
      imei: '012345678912345',
      mdn: '0123456789'
    });

    cellularCarrierHandler.onCarrierPortalStatusChange(
        chromeos.cellularSetup.mojom.CarrierPortalStatus
            .kPortalLoadedWithoutPaidUser);

    await flushAsync();
    assertTrue(pSimPage.nameOfCarrierPendingSetup === 'Verizon wireless');

    endFlowAndVerifyResult(PSimSetupFlowResult.CANCELLED);
  });

  test('forward navigation and finish cellular setup test', async function() {
    pSimPage.state_ = cellularSetup.PSimUIState.WAITING_FOR_PORTAL_TO_LOAD;
    Polymer.dom.flush();
    pSimPage.navigateForward();
    assertTrue(
        pSimPage.state_ ===
        cellularSetup.PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH);

    assertEquals(
        cellularSetup.ButtonState.ENABLED, pSimPage.buttonState.forward);
    assertEquals(pSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    pSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    pSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);

    endFlowAndVerifyResult(PSimSetupFlowResult.SUCCESS);
  });

  test('Already activated forward navigate exits cellular setup', async () => {
    cellularActivationDelegate =
        cellularSetupRemote.getLastActivationDelegate();
    cellularActivationDelegate.onActivationFinished(
        chromeos.cellularSetup.mojom.ActivationResult.kAlreadyActivated);

    await flushAsync();

    assertEquals(pSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    pSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    pSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);

    endFlowAndVerifyResult(PSimSetupFlowResult.SUCCESS);
  });

  test('Activation failure metric logged', async () => {
    cellularActivationDelegate =
        cellularSetupRemote.getLastActivationDelegate();

    let provisioningPage = pSimPage.$$('#provisioningPage');
    assertTrue(!!provisioningPage);
    assertFalse(
        pSimPage.selectedPSimPageName_ ===
        cellularSetup.PSimPageName.provisioningPage);

    cellularActivationDelegate.onActivationFinished(
        chromeos.cellularSetup.mojom.ActivationResult.kFailedToActivate);

    await flushAsync();
    endFlowAndVerifyResult(PSimSetupFlowResult.NETWORK_ERROR);
  });

  test('Portal error metric logged', () => {
    let provisioningPage = pSimPage.$$('#provisioningPage');
    provisioningPage.fire('carrier-portal-result', false);

    endFlowAndVerifyResult(PSimSetupFlowResult.CANCELLED_PORTAL_ERROR);
  });
});
