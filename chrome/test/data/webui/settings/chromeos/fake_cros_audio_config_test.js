// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {crosAudioConfigMojom, fakeCrosAudioConfig} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {MockController, MockMethod} from 'chrome://webui-test/mock_controller.js';

suite('FakeCrosAudioConfig', function() {
  // Observer for testing updates which have not been added to mojo yet.
  // Implements `fakeCrosAudioConfig.FakePropertiesObserverInterface`.
  class TestPropertiesObserver {
    onPropertiesUpdated(properties) {}
  }

  /** @type {crosAudioConfigMojom.AudioSystemProperties} */
  const defaultProperties =
      fakeCrosAudioConfig.defaultFakeAudioSystemProperties;

  /** @type {?fakeCrosAudioConfig.FakeCrosAudioConfig} */
  let crosAudioConfig = null;
  /** @type {?TestPropertiesObserver} */
  let fakeObserver = null;

  /** @type {?MockController} */
  let mockController = null;
  /** @type {?MockMethod} */
  let onPropertiesUpdated = null;

  setup(() => {
    crosAudioConfig = new fakeCrosAudioConfig.FakeCrosAudioConfig();
    fakeObserver = new TestPropertiesObserver();
    mockController = new MockController();
    onPropertiesUpdated =
        mockController.createFunctionMock(fakeObserver, 'onPropertiesUpdated');
    onPropertiesUpdated.addExpectation(defaultProperties);
    crosAudioConfig.observeAudioSystemProperties(fakeObserver);
  });

  teardown(() => {
    crosAudioConfig = null;
    fakeObserver = null;
    mockController = null;
    onPropertiesUpdated = null;
  });

  test('VerifyObserversReceiveAudioSystemProperitesUpdates', () => {
    // `fakeObserver` observer initialized during setup.
    assertDeepEquals(defaultProperties, onPropertiesUpdated.calls_[0][0]);

    /** @type {crosAudioConfigMojom.fakeObserver} */
    const observer = new TestPropertiesObserver();
    /** @type {MockMethod} */
    const onObserverPropertiesUpdated =
        mockController.createFunctionMock(observer, 'onPropertiesUpdated');

    assertEquals(0, onObserverPropertiesUpdated.calls_.length);
    crosAudioConfig.observeAudioSystemProperties(observer);

    assertDeepEquals(
        defaultProperties, onObserverPropertiesUpdated.calls_[0][0]);
    assertDeepEquals(defaultProperties, onPropertiesUpdated.calls_[1][0]);

    /** @type {crosAudioConfigMojom.AudioSystemProperties} */
    const updatedProperties = {
      outputDevices: [fakeCrosAudioConfig.fakeSpeakerActive],
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
    };
    crosAudioConfig.setAudioSystemProperties(updatedProperties);

    assertDeepEquals(
        updatedProperties, onObserverPropertiesUpdated.calls_[1][0]);
    assertDeepEquals(updatedProperties, onPropertiesUpdated.calls_[2][0]);
  });

  test('VerifySetOutputVolumeTriggersMatchingPropertyUpdate', () => {
    const outputVolumePercentExpected = 47;
    /** @type {AudioSystemProperties} */
    const updatedVolumeProperties = {
      ...defaultProperties,
      outputVolumePercent: outputVolumePercentExpected,
    };
    onPropertiesUpdated.addExpectation(updatedVolumeProperties);
    crosAudioConfig.setOutputVolumePercent(outputVolumePercentExpected);

    mockController.verifyMocks();
  });

  test(
      'VerifySetActiveDeviceOutputDeviceTriggersMatchingPropertyUpdate', () => {
        /** @type {AudioSystemProperties} */
        const updateActiveOutputDevice = {
          ...defaultProperties,
          outputDevices: [
            fakeCrosAudioConfig.createAudioDevice(
                fakeCrosAudioConfig.defaultFakeSpeaker, /*isActive=*/ true),
            fakeCrosAudioConfig.createAudioDevice(
                fakeCrosAudioConfig.defaultFakeMicJack, /*isActive=*/ false),
          ],
        };
        onPropertiesUpdated.addExpectation(updateActiveOutputDevice);
        crosAudioConfig.setActiveDevice(
            fakeCrosAudioConfig.defaultFakeSpeaker.id);

        mockController.verifyMocks();
      });

  test('VerifySetActiveDeviceInputDeviceTriggersMatchingPropertyUpdate', () => {
    /** @type {AudioSystemProperties} */
    const updateActiveInputDevice = {
      ...defaultProperties,
      inputDevices: [
        fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ false),
        fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.fakeBluetoothMic, /*isActive=*/ true),
      ],
    };
    onPropertiesUpdated.addExpectation(updateActiveInputDevice);
    crosAudioConfig.setActiveDevice(fakeCrosAudioConfig.fakeBluetoothMic.id);

    mockController.verifyMocks();
  });

  test('VerifySetOutputMutedTriggersMatchingPropertyUpdate', () => {
    /** @type {AudioSystemProperties} */
    const propertiesOutputMutedByUser = {
      ...defaultProperties,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
    };
    onPropertiesUpdated.addExpectation(propertiesOutputMutedByUser);
    crosAudioConfig.setOutputMuted(/*muted=*/ true);
    assertDeepEquals(
        propertiesOutputMutedByUser, onPropertiesUpdated.calls_[1][0]);

    /** @type {AudioSystemProperties} */
    const propertiesOutputUnmute = {
      ...defaultProperties,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };
    onPropertiesUpdated.addExpectation(propertiesOutputUnmute);
    crosAudioConfig.setOutputMuted(/*muted=*/ false);
    assertDeepEquals(propertiesOutputUnmute, onPropertiesUpdated.calls_[2][0]);
  });

  test('VerifySetInputMutedTriggersMatchingPropertyUpdate', () => {
    assertEquals(
        crosAudioConfigMojom.MuteState.kNotMuted,
        onPropertiesUpdated.calls_[0][0].inputMuteState);

    /** @type {AudioSystemProperties} */
    const updateInputGainMuted = {
      ...defaultProperties,
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
    };
    onPropertiesUpdated.addExpectation(updateInputGainMuted);
    crosAudioConfig.setInputMuted(/*muted=*/ true);

    assertEquals(
        crosAudioConfigMojom.MuteState.kMutedByUser,
        onPropertiesUpdated.calls_[1][0].inputMuteState);
    onPropertiesUpdated.addExpectation(defaultProperties);
    crosAudioConfig.setInputMuted(/*muted=*/ false);

    assertEquals(
        crosAudioConfigMojom.MuteState.kNotMuted,
        onPropertiesUpdated.calls_[2][0].inputMuteState);
  });

  test('VerifySetInputGainTriggersMatchingPropertyUpdate', () => {
    const expectedGainPercent = 32;
    /** @type {AudioSystemProperties} */
    const updatedProperties = {
      ...defaultProperties,
      inputGainPercent: expectedGainPercent,
    };
    onPropertiesUpdated.addExpectation(updatedProperties);
    crosAudioConfig.setInputGainPercent(expectedGainPercent);

    mockController.verifyMocks();
  });

  test(
      'VerifySetNoiseCancellationEnabledTriggersMatchingPropertyUpdate', () => {
        /** @type {AudioDevice} */
        const ncSupportedNotEnabled = fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ true);
        ncSupportedNotEnabled.noiseCancellationState =
            crosAudioConfigMojom.AudioEffectState.kNotEnabled;

        /** @type {AudioSystemProperties} */
        const defaultNoiseCancellation = {
          ...defaultProperties,
          inputDevices: [ncSupportedNotEnabled],
        };
        crosAudioConfig.setAudioSystemProperties(defaultNoiseCancellation);
        assertDeepEquals(
            defaultNoiseCancellation, onPropertiesUpdated.calls_[1][0]);
        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ true);

        /** @type {AudioDevice} */
        const ncSupportedEnabled = fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ true);
        ncSupportedEnabled.noiseCancellationState =
            crosAudioConfigMojom.AudioEffectState.kEnabled;

        /** @type {AudioSystemProperties} */
        const enabledNoiseCancellation = {
          ...defaultNoiseCancellation,
          inputDevices: [ncSupportedEnabled],
        };
        assertDeepEquals(
            enabledNoiseCancellation, onPropertiesUpdated.calls_[2][0]);

        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ false);
        assertDeepEquals(
            defaultNoiseCancellation, onPropertiesUpdated.calls_[2][0]);
      });

  test(
      'VerifySetNoiseCancellationEnabledDoesNotUpdateUnsupportedDevice', () => {
        /** @type {AudioDevice} */
        const ncNotSupported = fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ true);
        ncNotSupported.noiseCancellationState =
            crosAudioConfigMojom.AudioEffectState.kNotSupported;

        /** @type {AudioSystemProperties} */
        const noNoiseCancellation = {
          ...defaultProperties,
          inputDevices: [ncNotSupported],
        };
        crosAudioConfig.setAudioSystemProperties(noNoiseCancellation);
        assertDeepEquals(noNoiseCancellation, onPropertiesUpdated.calls_[1][0]);
        const expectedCallCount = 2;
        assertEquals(expectedCallCount, onPropertiesUpdated.calls_.length);
        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ true);
        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ false);
        assertEquals(expectedCallCount, onPropertiesUpdated.calls_.length);
      });
});
