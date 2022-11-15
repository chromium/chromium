// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/psim_flow_ui.js';

import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setCellularSetupRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {FAILED_PSIM_SETUP_DURATION_METRIC_NAME, PSimPageName, PSimSetupFlowResult, PSimUIState, SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/ash/common/cellular_setup/psim_flow_ui.js';
import {ActivationDelegateReceiver, ActivationResult, CarrierPortalStatus, CellularSetupRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';
import {FakeCarrierPortalHandlerRemote, FakeCellularSetupRemote} from './fake_cellular_setup_remote.js';
import {MockMetricsPrivate} from './mock_metrics_private.js';

suite('CrComponentsPsimFlowUiTest', function() {
  let pSimPage;

  /** @type {?CellularSetupRemote} */
  let cellularSetupRemote = null;

  /** @type {?FakeCarrierPortalHandlerRemote} */
  let cellularCarrierHandler = null;

  /** @type {?ActivationDelegateReceiver} */
  let cellularActivationDelegate = null;

  /** @type {function(Function, number)} */
  let timeoutFunction = null;

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /** @param {PSimSetupFlowResult} pSimSetupFlowResult */
  function endFlowAndVerifyResult(pSimSetupFlowResult) {
    pSimPage.remove();
    flush();
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

  setup(async function() {
    cellularCarrierHandler = new FakeCarrierPortalHandlerRemote();
    cellularSetupRemote = new FakeCellularSetupRemote(cellularCarrierHandler);
    setCellularSetupRemoteForTesting(cellularSetupRemote);
    chrome.metricsPrivate = new MockMetricsPrivate();

    pSimPage = document.createElement('psim-flow-ui');
    pSimPage.delegate = new FakeCellularSetupDelegate();
    pSimPage.setTimerFunctionForTest(function(fn, milliseconds) {
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
        cellularSetupRemote.getLastActivationDelegate();

    const provisioningPage = pSimPage.$$('#provisioningPage');
    assertTrue(!!provisioningPage);
    assertFalse(
        pSimPage.selectedPSimPageName_ === PSimPageName.provisioningPage);

    cellularActivationDelegate.onActivationFinished(
        ActivationResult.kSuccessfullyStartedActivation);

    await flushAsync();

    assertTrue(pSimPage.selectedPSimPageName_ === PSimPageName.PROVISIONING);

    endFlowAndVerifyResult(PSimSetupFlowResult.SUCCESS);
  });

  test('Sim detection failure with retries', async function() {
    assertTrue(pSimPage.state_ === PSimUIState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.currentTimeoutId_);

    await flushAsync();

    // Simulate timeout.
    timeoutFunction();

    assertTrue(pSimPage.state_ === PSimUIState.TIMEOUT_START_ACTIVATION);
    assertTrue(pSimPage.forwardButtonLabel === 'Try again');

    // Simulate clicking 'Try Again'.
    pSimPage.navigateForward();

    assertTrue(pSimPage.state_ === PSimUIState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.currentTimeoutId_);

    await flushAsync();

    // Timeout again.
    timeoutFunction();

    assertTrue(pSimPage.state_ === PSimUIState.TIMEOUT_START_ACTIVATION);
    assertTrue(pSimPage.forwardButtonLabel === 'Try again');

    // Click 'Try Again' again.
    pSimPage.navigateForward();

    assertTrue(pSimPage.state_ === PSimUIState.STARTING_ACTIVATION);
    assertTrue(!!pSimPage.currentTimeoutId_);

    await flushAsync();

    // Timeout again.
    timeoutFunction();

    // Should now be at the failure state.
    assertTrue(pSimPage.state_ === PSimUIState.FINAL_TIMEOUT_START_ACTIVATION);
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
      mdn: '0123456789',
    });

    cellularCarrierHandler.onCarrierPortalStatusChange(
        CarrierPortalStatus.kPortalLoadedWithoutPaidUser);

    await flushAsync();
    assertTrue(pSimPage.nameOfCarrierPendingSetup === 'Verizon wireless');

    endFlowAndVerifyResult(PSimSetupFlowResult.CANCELLED);
  });

  test('forward navigation and finish cellular setup test', async function() {
    pSimPage.state_ = PSimUIState.WAITING_FOR_PORTAL_TO_LOAD;
    flush();
    pSimPage.navigateForward();
    assertTrue(
        pSimPage.state_ === PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH);

    assertEquals(ButtonState.ENABLED, pSimPage.buttonState.forward);
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

    endFlowAndVerifyResult(PSimSetupFlowResult.SUCCESS);
  });

  test('Activation failure metric logged', async () => {
    cellularActivationDelegate =
        cellularSetupRemote.getLastActivationDelegate();

    const provisioningPage = pSimPage.$$('#provisioningPage');
    assertTrue(!!provisioningPage);
    assertFalse(
        pSimPage.selectedPSimPageName_ === PSimPageName.provisioningPage);

    cellularActivationDelegate.onActivationFinished(
        ActivationResult.kFailedToActivate);

    await flushAsync();
    endFlowAndVerifyResult(PSimSetupFlowResult.NETWORK_ERROR);
  });

  test('Portal error metric logged', () => {
    const provisioningPage = pSimPage.$$('#provisioningPage');
    provisioningPage.fire('carrier-portal-result', false);

    endFlowAndVerifyResult(PSimSetupFlowResult.CANCELLED_PORTAL_ERROR);
  });
});
