// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, ComponentRepairStatus, ComponentType, ErrorObserverRemote, FinalizationError, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningError, ProvisioningObserverRemote, ProvisioningStatus, RmadErrorCode, ShutdownMethod, State, UpdateErrorCode, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeShimlessRmaServiceTestSuite', function() {
  let service: FakeShimlessRmaService|null = null;

  const expectedProgressPercentage = 0.5;

  setup(() => {
    service = new FakeShimlessRmaService();
  });

  teardown(() => {
    service = null;
  });

  // Verify the service begins in state `State.kUnknown`.
  test('GetCurrentStateDefaultRmaNotRequired', async () => {
    assert(service);
    const result = await service.getCurrentState();
    assertEquals(State.kUnknown, result.stateResult.state);
    assertEquals(RmadErrorCode.kRmaNotRequired, result.stateResult.error);
  });

  // Verify the first set state is the current state.
  test('GetCurrentState', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.getCurrentState();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertTrue(result.stateResult.canExit);
    assertFalse(result.stateResult.canGoBack);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `getCurrentState()` returns the set error.
  test('GetCurrentStateError', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kMissingComponent,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.getCurrentState();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kMissingComponent, result.stateResult.error);
  });

  // Verify transitioning back to the previous state.
  test('TransitionPreviousState', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    let result = await service.beginFinalization();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);

    result = await service.transitionPreviousState();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify transitioning back fails if there's no previous state.
  test('TransitionPreviousStateFailed', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.transitionPreviousState();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kTransitionFailed, result.stateResult.error);
  });

  // Verify transitioning states fails if no state is set.
  test('TransitionNoStatesFailed', async () => {
    assert(service);
    const result = await service.beginFinalization();
    assertEquals(State.kUnknown, result.stateResult.state);
    assertEquals(RmadErrorCode.kRmaNotRequired, result.stateResult.error);
  });

  // Verify the `abortRma()` error code can be set.
  test('SetAbortRmaResultUpdatesResult', async () => {
    assert(service);
    service.setAbortRmaResult(RmadErrorCode.kRequestInvalid);
    const result = await service.abortRma();
    assertEquals(RmadErrorCode.kRequestInvalid, result.error);
  });

  // Verify the `getCurrentOsVersion()` version can be set.
  test('SetGetCurrentOsVersionResultUpdatesResult', async () => {
    const expectedVersionResult = '1234.56.78';
    assert(service);
    service.setGetCurrentOsVersionResult(expectedVersionResult);
    const result = await service.getCurrentOsVersion();
    assertEquals(expectedVersionResult, result.version);
  });

  // Verify the `updateOs()` result can be set.
  test('SetUpdateOsResultTrueUpdatesResult', async () => {
    assert(service);
    service.setUpdateOsResult(true);

    let result = await service.updateOs();
    assertTrue(result.updateStarted);

    service.setUpdateOsResult(false);
    result = await service.updateOs();
    assertFalse(result.updateStarted);
  });

  // Verify `updateOsSkipped()` goes to the next state.
  test('UpdateOsSkippedOk', async () => {
    const states = [
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.updateOsSkipped();
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `updateOsSkipped()` can't be called from the wrong state.
  test('UpdateOsSkippedWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.updateOsSkipped();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify `setSameOwner()` can be called from the correct state.
  test('SetSameOwnerOk', async () => {
    const states = [
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setSameOwner();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `setSameOwner()` can't be called from the wrong state.
  test('SetSameOwnerWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setSameOwner();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify `setDifferentOwner()` can be called from the correct state.
  test('SetDifferentOwnerOk', async () => {
    const states = [
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setDifferentOwner();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `setDifferentOwner()` can't be called from the wrong state.
  test('SetDifferentOwnerWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setDifferentOwner();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify `setManuallyDisableWriteProtect()` can be called from the correct
  // state.
  test('SetManuallyDisableWriteProtectOk', async () => {
    const states = [
      {
        state: State.kChooseWriteProtectDisableMethod,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setManuallyDisableWriteProtect();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `setManuallyDisableWriteProtect()` can't be called from the wrong
  // state.
  test('SetManuallyDisableWriteProtectWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setManuallyDisableWriteProtect();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify `setRsuDisableWriteProtect()` can be called from the correct
  // state.
  test('SetRsuDisableWriteProtectOk', async () => {
    const states = [
      {
        state: State.kChooseWriteProtectDisableMethod,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setRsuDisableWriteProtect();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `setRsuDisableWriteProtect()` can't be called from the wrong
  // state.
  test('SetRsuDisableWriteProtectWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setRsuDisableWriteProtect();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify the `getRsuDisableWriteProtectChallenge()` challenge result can be
  // set.
  test('SetGetRsuDisableWriteProtectChallengeResultUpdatesResult', async () => {
    const expectedChallenge = '9876543210';
    assert(service);
    service.setGetRsuDisableWriteProtectChallengeResult(expectedChallenge);
    const result = await service.getRsuDisableWriteProtectChallenge();
    assertEquals(expectedChallenge, result.challenge);
  });

  // Verify `setRsuDisableWriteProtectCode()` can be called from the correct
  // state.
  test('SetRsuDisableWriteProtectCodeOk', async () => {
    const states = [
      {
        state: State.kEnterRSUWPDisableCode,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setRsuDisableWriteProtectCode(/*code=*/ '');
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `setRsuDisableWriteProtectCode()` can't be called from the wrong
  // state.
  test('SetRsuDisableWriteProtectCodeWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setRsuDisableWriteProtectCode(/*code=*/ '');
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify the list of components can be set.
  test('SetGetComponentListResultUpdatesResult', async () => {
    const expectedComponents = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal,
        identifier: 'keyboard',
      },
      {
        component: ComponentType.kTouchpad,
        state: ComponentRepairStatus.kMissing,
        identifier: 'touchpad',
      },
    ];
    assert(service);
    service.setGetComponentListResult(expectedComponents);
    const result = await service.getComponentList();
    assertDeepEquals(expectedComponents, result.components);
  });

  // Verify `setComponentList()` can be called from the correct state.
  test('SetComponentListOk', async () => {
    const states = [
      {
        state: State.kSelectComponents,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setComponentList([]);
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `setComponentList()` can't be called from the wrong state.
  test('SetComponentListWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setComponentList([]);
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify `reworkMainboard()` can be called from the correct state.
  test('ReworkMainboardOk', async () => {
    const states = [
      {
        state: State.kSelectComponents,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.reworkMainboard();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `reworkMainboard()` can't be called from the wrong state.
  test('ReworkMainboardWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.reworkMainboard();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify the `getWriteProtectDisableCompleteAction()` action can be set.
  test('SetGetWriteProtectDisableCompleteStateUpdatesAction', async () => {
    assert(service);
    service.setGetWriteProtectDisableCompleteAction(
        WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen);
    const result = await service.getWriteProtectDisableCompleteAction();
    assertEquals(
        WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen,
        result.action);
  });

  // Verify `roFirmwareUpdateComplete()` can be called from the correct state.
  test('ReimageRoFirmwareUpdateCompleteOk', async () => {
    const states = [
      {
        state: State.kUpdateRoFirmware,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.roFirmwareUpdateComplete();
    assertEquals(State.kUpdateOs, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `roFirmwareUpdateComplete()` can't be called from the wrong state.
  test('ReimageRoFirmwareUpdateCompleteWrongStateFails', async () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kUpdateOs,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.roFirmwareUpdateComplete();
    assertEquals(State.kWelcomeScreen, result.stateResult.state);
    assertEquals(RmadErrorCode.kRequestInvalid, result.stateResult.error);
  });

  // Verify the region list can be set.
  test('SetGetRegionListResultUpdatesResult', async () => {
    const regionList = ['America', 'Asia', 'Europe'];
    assert(service);
    service.setGetRegionListResult(regionList);
    const result = await service.getRegionList();
    assertDeepEquals(regionList, result.regions);
  });

  // Verify the SKU list can be set.
  test('SetGetSkuListResultUpdatesResult', async () => {
    const skuList = [BigInt(1), BigInt(202), BigInt(33)];
    assert(service);
    service.setGetSkuListResult(skuList);
    const result = await service.getSkuList();
    assertDeepEquals(skuList, result.skus);
  });

  // Verify the custom label list can be set.
  test('SetGetCustomLabelListResultUpdatesResult', async () => {
    const customLabelList =
        ['Custom-label 10', 'Custom-label 0', 'Custom-label 9999'];
    assert(service);
    service.setGetCustomLabelListResult(customLabelList);
    const result = await service.getCustomLabelList();
    assertDeepEquals(customLabelList, result.customLabels);
  });

  // Verify the original serial number can be set.
  test('SetGetOriginalSerialNumberResultUpdatesResult', async () => {
    const expectedSerialNumber = '123456789';
    assert(service);
    service.setGetOriginalSerialNumberResult(expectedSerialNumber);
    const result = await service.getOriginalSerialNumber();
    assertEquals(expectedSerialNumber, result.serialNumber);
  });

  // Verify the original region can be set.
  test('SetGetOriginalRegionResultUpdatesResult', async () => {
    const expectedRegion = 1;
    assert(service);
    service.setGetOriginalRegionResult(expectedRegion);
    const result = await service.getOriginalRegion();
    assertEquals(expectedRegion, result.regionIndex);
  });

  // Verify the original SKU can be set.
  test('SetGetOriginalSkuResultUpdatesResult', async () => {
    const expectedSku = 1;
    assert(service);
    service.setGetOriginalSkuResult(expectedSku);
    const result = await service.getOriginalSku();
    assertEquals(expectedSku, result.skuIndex);
  });

  // Verify the original custom label can be set.
  test('SetGetOriginalCustomLabelResultUpdatesResult', async () => {
    const expectedCustomLabel = 1;
    assert(service);
    service.setGetOriginalCustomLabelResult(expectedCustomLabel);
    const result = await service.getOriginalCustomLabel();
    assertEquals(expectedCustomLabel, result.customLabelIndex);
  });

  // Verify the original DRAM part number can be set.
  test('SetGetOriginalDramPartNumberResultUpdatesResult', async () => {
    const expectedDramPartNumber = '123-456-789';
    assert(service);
    service.setGetOriginalDramPartNumberResult(expectedDramPartNumber);
    const result = await service.getOriginalDramPartNumber();
    assertEquals(expectedDramPartNumber, result.dramPartNumber);
  });

  // Verify `setDeviceInformation()` can be called from the correct state.
  test('SetDeviceInformationOk', async () => {
    const states = [
      {
        state: State.kUpdateDeviceInformation,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.setDeviceInformation(
        'serial number', 1, 2, 3, '123-456-789', false, 1);
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify the calibration components list can be set.
  test('GetCalibrationComponentList', async () => {
    assert(service);
    const expectedCalibrationComponents = [
      {
        component: ComponentType.kLidAccelerometer,
        status: CalibrationStatus.kCalibrationInProgress,
        progress: expectedProgressPercentage,
      },
      {
        component: ComponentType.kBaseAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: expectedProgressPercentage,
      },
    ];
    service.setGetCalibrationComponentListResult(expectedCalibrationComponents);

    const result = await service.getCalibrationComponentList();
    assertDeepEquals(expectedCalibrationComponents, result.components);
  });

  // Verify the calibration instructions can be set.
  test('GetCalibrationInstructions', async () => {
    assert(service);
    service.setGetCalibrationSetupInstructionsResult(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    const result = await service.getCalibrationSetupInstructions();
    assertEquals(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface,
        result.instructions);
  });

  // Verify `startCalibration()` can be called from the correct state.
  test('StartCalibrationOk', async () => {
    const states = [
      {
        state: State.kCheckCalibration,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.startCalibration(/* components= */[]);
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `runCalibrationStep()` can be called from the correct state.
  test('RunCalibrationStepOk', async () => {
    const states = [
      {
        state: State.kSetupCalibration,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.runCalibrationStep();
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `continueCalibration()` can be called from the correct state.
  test('ContinueCalibrationOk', async () => {
    const states = [
      {
        state: State.kRunCalibration,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.continueCalibration();
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify `calibrationComplete()` can be called from the correct state.
  test('CalibrationCompleteOk', async () => {
    const states = [
      {
        state: State.kRunCalibration,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.calibrationComplete();
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify the log can be set.
  test('GetLog', async () => {
    assert(service);
    const expectedLog = 'fake log';
    service.setGetLogResult(expectedLog);
    const result = await service.getLog();
    assertEquals(expectedLog, result.log);
  });

  // Verify the log save path can be set.
  test('SaveLog', async () => {
    assert(service);
    const expectedSavePath = {'path': 'fake/save/path'};
    service.setSaveLogResult(expectedSavePath);
    const result = await service.saveLog();
    assertEquals(expectedSavePath, result.savePath);
  });

  // Verify the `getPowerwashRequired()` result can be set.
  test('SetGetPowerwashRequiredResultTrueUpdatesResult', async () => {
    assert(service);
    service.setGetPowerwashRequiredResult(true);

    const result = await service.getPowerwashRequired();
    assertTrue(result.powerwashRequired);
  });

  // Verify `endRma()` can be called from the correct state.
  test('EndRma', async () => {
    const states = [
      {
        state: State.kRepairComplete,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
      {
        state: State.kChooseDestination,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    assert(service);
    service.setStates(states);

    const result = await service.endRma(ShutdownMethod.kShutdown);
    assertEquals(State.kChooseDestination, result.stateResult.state);
    assertEquals(RmadErrorCode.kOk, result.stateResult.error);
  });

  // Verify the error observer is triggered.
  test('ObserveError', async () => {
    const errorObserver = {
      onError(error): void {
        assertEquals(RmadErrorCode.kRequestInvalid, error);
      },
    } as ErrorObserverRemote;

    assert(service);
    service.observeError(errorObserver);
    await service.triggerErrorObserver(
        RmadErrorCode.kRequestInvalid, /*delayMs=*/ 0);
  });

  // Verify the OS update progress observer is triggered.
  test('ObserveOsUpdate', async () => {
    const osUpdateObserver = {
      onOsUpdateProgressUpdated(operation, progress, updateErrorCode): void {
        assertEquals(OsUpdateOperation.kDownloading, operation);
        assertEquals(expectedProgressPercentage, progress);
        assertEquals(UpdateErrorCode.kSuccess, updateErrorCode);
      },
    } as OsUpdateObserverRemote;

    assert(service);
    service.observeOsUpdateProgress(osUpdateObserver);
    await service.triggerOsUpdateObserver(
        OsUpdateOperation.kDownloading, expectedProgressPercentage,
        UpdateErrorCode.kSuccess, /*delayMs=*/ 0);
  });

  // Verify the RO firmware status observer is triggered.
  test('ObserveRoFirmwareUpdate', async () => {
    const roFirmwareUpdateObserver = {
      onUpdateRoFirmwareStatusChanged(status): void {
        assertEquals(UpdateRoFirmwareStatus.kDownloading, status);
      },
    } as UpdateRoFirmwareObserverRemote;

    assert(service);
    service.observeRoFirmwareUpdateProgress(roFirmwareUpdateObserver);
    await service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kDownloading, /*delayMs=*/ 0);
  });

  // Verify the calibration observer is triggered.
  test('ObserveCalibrationUpdated', async () => {
    const calibrationObserver = {
      onCalibrationUpdated(calibrationStatus): void {
        assertEquals(
            ComponentType.kBaseAccelerometer, calibrationStatus.component);
        assertEquals(
            CalibrationStatus.kCalibrationComplete, calibrationStatus.status);
        assertEquals(expectedProgressPercentage, calibrationStatus.progress);
      },

      onCalibrationStepComplete(status): void {
        assertEquals(
            CalibrationOverallStatus.kCalibrationOverallComplete, status);
      },
    } as CalibrationObserverRemote;

    assert(service);
    service.observeCalibrationProgress(calibrationObserver);
    await service.triggerCalibrationObserver(
        {
          component: ComponentType.kBaseAccelerometer,
          status: CalibrationStatus.kCalibrationComplete,
          progress: expectedProgressPercentage,
        },
        /*delayMs=*/ 0);

    await service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, /*delayMs=*/ 0);
  });

  // Verify the provisioning observer is triggered.
  test('ObserveProvisioningUpdated', async () => {
    const provisioningObserver = {
      onProvisioningUpdated(status, progress, error): void {
        assertEquals(ProvisioningStatus.kInProgress, status);
        assertEquals(expectedProgressPercentage, progress);
        assertEquals(ProvisioningError.kUnknown, error);
      },
    } as ProvisioningObserverRemote;

    assert(service);
    service.observeProvisioningProgress(provisioningObserver);
    await service.triggerProvisioningObserver(
        ProvisioningStatus.kInProgress, expectedProgressPercentage,
        ProvisioningError.kUnknown, /*delayMs=*/ 0);
  });

  // Verify the hardware write protection state observer is triggered.
  test('ObserveHardwareWriteProtectionStateChange', async () => {
    const hardwareWriteProtectionStateObserver = {
      onHardwareWriteProtectionStateChanged(enable) {
        assertTrue(enable);
      },
    } as HardwareWriteProtectionStateObserverRemote;

    assert(service);
    service.observeHardwareWriteProtectionState(
        hardwareWriteProtectionStateObserver);
    await service.triggerHardwareWriteProtectionObserver(true, /*delayMs=*/ 0);
  });

  // Verify the power cable state observer is triggered.
  test('ObservePowerCableStateChange', async () => {
    const powerCableStateObserver = {
      onPowerCableStateChanged(enable): void {
        assertTrue(enable);
      },
    } as PowerCableStateObserverRemote;

    assert(service);
    service.observePowerCableState(powerCableStateObserver);
    await service.triggerPowerCableObserver(true, /*delayMs=*/ 0);
  });

  // Verify the hardware verification status observer is triggered.
  test('ObserveHardwareVerificationStatus', async () => {
    const observer = {
      onHardwareVerificationResult(isCompliant, errorMessage): void {
        assertTrue(isCompliant);
        assertEquals('ok', errorMessage);
      },
    } as HardwareVerificationStatusObserverRemote;

    assert(service);
    service.observeHardwareVerificationStatus(observer);
    await service.triggerHardwareVerificationStatusObserver(
        true, 'ok', /*delayMs=*/ 0);
  });

  // Verify the finalization observer is triggered.
  test('ObserveFinalizationStatus', async () => {
    const finalizationObserver = {
      onFinalizationUpdated(status, progress, error): void {
        assertEquals(FinalizationStatus.kInProgress, status);
        assertEquals(expectedProgressPercentage, progress);
        assertEquals(FinalizationError.kUnknown, error);
      },
    } as FinalizationObserverRemote;

    assert(service);
    service.observeFinalizationStatus(finalizationObserver);
    await service.triggerFinalizationObserver(
        FinalizationStatus.kInProgress, expectedProgressPercentage,
        FinalizationError.kUnknown, /*delayMs=*/ 0);
  });
});
