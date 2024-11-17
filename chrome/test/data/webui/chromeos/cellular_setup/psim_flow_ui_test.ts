// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/psim_flow_ui.js';

import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setCellularSetupRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import type {PsimFlowUiElement} from 'chrome://resources/ash/common/cellular_setup/psim_flow_ui.js';
import {FAILED_PSIM_SETUP_DURATION_METRIC_NAME, PsimPageName, PsimSetupFlowResult, PsimUiState, SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/ash/common/cellular_setup/psim_flow_ui.js';
import type {ActivationDelegateRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {ActivationResult, CarrierPortalStatus} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';
import {FakeCarrierPortalHandlerRemote, FakeCellularSetupRemote} from './fake_cellular_setup_remote.js';
import {MockMetricsPrivate} from './mock_metrics_private.js';

suite('CrComponentsPsimFlowUiTest', function() {
  let pSimPage: PsimFlowUiElement;
  let cellularSetupRemote: FakeCellularSetupRemote|null;
  let cellularCarrierHandler: FakeCarrierPortalHandlerRemote|null;
  let cellularActivationDelegate: ActivationDelegateRemote|null;
  let timeoutFunction: Function|null;
  let metrics: MockMetricsPrivate;

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function endFlowAndVerifyResult(psimSetupFlowResult: PsimSetupFlowResult) {
    pSimPage.remove();
    flush();
    assertEquals(metrics.getHistogramEnumValueCount(psimSetupFlowResult), 1);

    if (psimSetupFlowResult === PsimSetupFlowResult.SUCCESS) {
      assertEquals(
          metrics.getHistogramCount(FAILED_PSIM_SETUP_DURATION_METRIC_NAME), 0);
      assertEquals(
          metrics.getHistogramCount(SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME),
          1);
      return;
    }

    assertEquals(
        metrics.getHistogramCount(FAILED_PSIM_SETUP_DURATION_METRIC_NAME), 1);
    assertEquals(
        metrics.getHistogramCount(SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME),
        0);
  }

  setup(async function() {
    cellularCarrierHandler = new FakeCarrierPortalHandlerRemote();
    cellularSetupRemote = new FakeCellularSetupRemote(cellularCarrierHandler);
    setCellularSetupRemoteForTesting(cellularSetupRemote);
    metrics = new MockMetricsPrivate();
    chrome.metricsPrivate = metrics as unknown as typeof chrome.metricsPrivate;
    flush();

    pSimPage = document.createElement('psim-flow-ui');
    pSimPage.delegate = new FakeCellularSetupDelegate();
    pSimPage.setTimerFunctionForTest(function(fn: Function, _: number) {
      timeoutFunction = fn;
      return 1;
    });

    const focusNextButtonPromise =
        eventToPromise('focus-default-button', pSimPage);
    pSimPage.initSubflow();
    await focusNextButtonPromise;
    document.body.appendChild(pSimPage);
    flush();
  });

  teardown(function() {
    pSimPage.remove();
  });

  test('Show provisioning page on activation finished', async () => {
    cellularActivationDelegate =
        cellularSetupRemote!.getLastActivationDelegate();

    const provisioningPage =
        pSimPage.shadowRoot!.querySelector('#provisioningPage');
    assertTrue(!!provisioningPage);
    assertFalse(
        pSimPage.getSelectedPsimPageNameForTest() ===
        PsimPageName.PROVISIONING);

    assertTrue(!!cellularActivationDelegate);
    cellularActivationDelegate.onActivationFinished(
        ActivationResult.kSuccessfullyStartedActivation);

    await flushAsync();

    assertTrue(
        pSimPage.getSelectedPsimPageNameForTest() ===
        PsimPageName.PROVISIONING);

    endFlowAndVerifyResult(PsimSetupFlowResult.SUCCESS);
  });

  test('Sim detection failure with retries', async function() {
    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.getCurrentTimeoutIdForTest());

    await flushAsync();

    // Simulate timeout.
    assertTrue(!!timeoutFunction);
    timeoutFunction();

    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.TIMEOUT_START_ACTIVATION);
    assertTrue(pSimPage.forwardButtonLabel === 'Try again');

    // Simulate clicking 'Try Again'.
    pSimPage.navigateForward();

    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.getCurrentTimeoutIdForTest());

    await flushAsync();

    // Timeout again.
    assertTrue(!!timeoutFunction);
    timeoutFunction();

    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.TIMEOUT_START_ACTIVATION);
    assertTrue(pSimPage.forwardButtonLabel === 'Try again');

    // Click 'Try Again' again.
    pSimPage.navigateForward();

    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.getCurrentTimeoutIdForTest());

    await flushAsync();

    // Timeout again.
    assertTrue(!!timeoutFunction);
    timeoutFunction();

    // Should now be at the failure state.
    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.FINAL_TIMEOUT_START_ACTIVATION);
  });

  test('Carrier title on provisioning page', async () => {
    cellularActivationDelegate =
        cellularSetupRemote!.getLastActivationDelegate();

    assertTrue(!!cellularActivationDelegate);
    cellularActivationDelegate.onActivationStarted({
      paymentUrl: {url: ''},
      paymentPostData: 'verison_post_data',
      carrier: 'Verizon wireless',
      meid: '012345678912345',
      imei: '012345678912345',
      mdn: '0123456789',
    });

    assertTrue(!!cellularCarrierHandler);
    cellularCarrierHandler.onCarrierPortalStatusChange(
        CarrierPortalStatus.kPortalLoadedWithoutPaidUser);

    await flushAsync();
    assertTrue(pSimPage.nameOfCarrierPendingSetup === 'Verizon wireless');

    endFlowAndVerifyResult(PsimSetupFlowResult.CANCELLED);
  });

  test('forward navigation and finish cellular setup test', async function() {
    pSimPage.setCurrentPsimUiStateForTest(
        PsimUiState.WAITING_FOR_PORTAL_TO_LOAD);
    flush();
    pSimPage.navigateForward();
    assertTrue(
        pSimPage.getCurrentPsimUiStateForTest() ===
        PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH);

    assertEquals(ButtonState.ENABLED, pSimPage.buttonState.forward);
    assertEquals(pSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    pSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    pSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);

    endFlowAndVerifyResult(PsimSetupFlowResult.SUCCESS);
  });

  test('Already activated forward navigate exits cellular setup', async () => {
    cellularActivationDelegate =
        cellularSetupRemote!.getLastActivationDelegate();
    assertTrue(!!cellularActivationDelegate);
    cellularActivationDelegate.onActivationFinished(
        ActivationResult.kAlreadyActivated);

    await flushAsync();

    assertEquals(pSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    pSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    pSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);

    endFlowAndVerifyResult(PsimSetupFlowResult.SUCCESS);
  });

  test('Activation failure metric logged', async () => {
    cellularActivationDelegate =
        cellularSetupRemote!.getLastActivationDelegate();

    const provisioningPage =
        pSimPage.shadowRoot!.querySelector('#provisioningPage');
    assertTrue(!!provisioningPage);
    assertFalse(
        pSimPage.getSelectedPsimPageNameForTest() ===
        PsimPageName.PROVISIONING);

    assertTrue(!!cellularActivationDelegate);
    cellularActivationDelegate.onActivationFinished(
        ActivationResult.kFailedToActivate);

    await flushAsync();
    endFlowAndVerifyResult(PsimSetupFlowResult.NETWORK_ERROR);
  });

  test('Portal error metric logged', () => {
    const provisioningPage =
        pSimPage.shadowRoot!.querySelector('#provisioningPage');
    assertTrue(!!provisioningPage);
    provisioningPage.dispatchEvent(new CustomEvent(
        'carrier-portal-result',
        {bubbles: true, composed: true, detail: false}));

    endFlowAndVerifyResult(PsimSetupFlowResult.CANCELLED_PORTAL_ERROR);
  });
});
