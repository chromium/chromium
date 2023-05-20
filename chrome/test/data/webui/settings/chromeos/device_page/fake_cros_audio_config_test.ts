// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {crosAudioConfigMojom, fakeCrosAudioConfig} from 'chrome://os-settings/os_settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockController, MockMethod} from 'chrome://webui-test/mock_controller.js';

suite('FakeCrosAudioConfig', () => {
  // Observer for testing updates which have not been added to mojo yet.
  class TestPropertiesObserver implements
      fakeCrosAudioConfig.FakePropertiesObserverInterface {
    onPropertiesUpdated() {}
  }

  const defaultProperties: crosAudioConfigMojom.AudioSystemProperties =
      fakeCrosAudioConfig.defaultFakeAudioSystemProperties;

  let crosAudioConfig: fakeCrosAudioConfig.FakeCrosAudioConfig;
  let fakeObserver: TestPropertiesObserver;
  let mockController: MockController;
  let onPropertiesUpdated: MockMethod;

  setup(() => {
    crosAudioConfig = new fakeCrosAudioConfig.FakeCrosAudioConfig();
    fakeObserver = new TestPropertiesObserver();
    mockController = new MockController();
    onPropertiesUpdated =
        mockController.createFunctionMock(fakeObserver, 'onPropertiesUpdated');
    onPropertiesUpdated.addExpectation(defaultProperties);
    crosAudioConfig.observeAudioSystemProperties(fakeObserver);
    assertTrue(!!onPropertiesUpdated);
  });

  test('VerifyObserversReceiveAudioSystemProperitesUpdates', () => {
    // `fakeObserver` observer initialized during setup.
    assertDeepEquals(defaultProperties, onPropertiesUpdated['calls_'][0]![0]);

    const observer = new TestPropertiesObserver();
    const onObserverPropertiesUpdated: MockMethod =
        mockController.createFunctionMock(observer, 'onPropertiesUpdated');

    assertTrue(!!onObserverPropertiesUpdated);
    assertEquals(0, onObserverPropertiesUpdated['calls_'].length);
    crosAudioConfig.observeAudioSystemProperties(observer);

    assertDeepEquals(
        defaultProperties, onObserverPropertiesUpdated['calls_'][0]![0]);
    assertDeepEquals(defaultProperties, onPropertiesUpdated['calls_'][1]![0]);

    const updatedProperties: crosAudioConfigMojom.AudioSystemProperties = {
      outputDevices: [fakeCrosAudioConfig.fakeSpeakerActive],
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
      inputGainPercent: 0,
      inputDevices: [],
      inputMuteState: crosAudioConfigMojom.MuteState.MIN_VALUE,
    };
    crosAudioConfig.setAudioSystemProperties(updatedProperties);

    assertDeepEquals(
        updatedProperties, onObserverPropertiesUpdated['calls_'][1]![0]);
    assertDeepEquals(updatedProperties, onPropertiesUpdated['calls_'][2]![0]);
  });

  test('VerifySetOutputVolumeTriggersMatchingPropertyUpdate', () => {
    const outputVolumePercentExpected = 47;
    const updatedVolumeProperties:
        crosAudioConfigMojom.AudioSystemProperties = {
      ...defaultProperties,
      outputVolumePercent: outputVolumePercentExpected,
    };
    onPropertiesUpdated.addExpectation(updatedVolumeProperties);
    crosAudioConfig.setOutputVolumePercent(outputVolumePercentExpected);

    mockController.verifyMocks();
  });

  test(
      'VerifySetActiveDeviceOutputDeviceTriggersMatchingPropertyUpdate', () => {
        const updateActiveOutputDevice:
            crosAudioConfigMojom.AudioSystemProperties = {
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
    const updateActiveInputDevice:
        crosAudioConfigMojom.AudioSystemProperties = {
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
    const propertiesOutputMutedByUser:
        crosAudioConfigMojom.AudioSystemProperties = {
      ...defaultProperties,
      outputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
    };
    onPropertiesUpdated.addExpectation(propertiesOutputMutedByUser);
    crosAudioConfig.setOutputMuted(/*muted=*/ true);
    assertDeepEquals(
        propertiesOutputMutedByUser, onPropertiesUpdated['calls_'][1]![0]);

    const propertiesOutputUnmute: crosAudioConfigMojom.AudioSystemProperties = {
      ...defaultProperties,
      outputMuteState: crosAudioConfigMojom.MuteState.kNotMuted,
    };
    onPropertiesUpdated.addExpectation(propertiesOutputUnmute);
    crosAudioConfig.setOutputMuted(/*muted=*/ false);
    assertDeepEquals(
        propertiesOutputUnmute, onPropertiesUpdated['calls_'][2]![0]);
  });

  test('VerifySetInputMutedTriggersMatchingPropertyUpdate', () => {
    assertEquals(
        crosAudioConfigMojom.MuteState.kNotMuted,
        onPropertiesUpdated['calls_'][0]![0].inputMuteState);

    const updateInputGainMuted: crosAudioConfigMojom.AudioSystemProperties = {
      ...defaultProperties,
      inputMuteState: crosAudioConfigMojom.MuteState.kMutedByUser,
    };
    onPropertiesUpdated.addExpectation(updateInputGainMuted);
    crosAudioConfig.setInputMuted(/*muted=*/ true);

    assertEquals(
        crosAudioConfigMojom.MuteState.kMutedByUser,
        onPropertiesUpdated['calls_'][1]![0].inputMuteState);
    onPropertiesUpdated.addExpectation(defaultProperties);
    crosAudioConfig.setInputMuted(/*muted=*/ false);

    assertEquals(
        crosAudioConfigMojom.MuteState.kNotMuted,
        onPropertiesUpdated['calls_'][2]![0].inputMuteState);
  });

  test('VerifySetInputGainTriggersMatchingPropertyUpdate', () => {
    const expectedGainPercent = 32;
    const updatedProperties: crosAudioConfigMojom.AudioSystemProperties = {
      ...defaultProperties,
      inputGainPercent: expectedGainPercent,
    };
    onPropertiesUpdated.addExpectation(updatedProperties);
    crosAudioConfig.setInputGainPercent(expectedGainPercent);

    mockController.verifyMocks();
  });

  test(
      'VerifySetNoiseCancellationEnabledTriggersMatchingPropertyUpdate', () => {
        const ncSupportedNotEnabled: crosAudioConfigMojom.AudioDevice =
            fakeCrosAudioConfig.createAudioDevice(
                fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ true);
        ncSupportedNotEnabled.noiseCancellationState =
            crosAudioConfigMojom.AudioEffectState.kNotEnabled;

        const defaultNoiseCancellation:
            crosAudioConfigMojom.AudioSystemProperties = {
          ...defaultProperties,
          inputDevices: [ncSupportedNotEnabled],
        };
        crosAudioConfig.setAudioSystemProperties(defaultNoiseCancellation);
        assertDeepEquals(
            defaultNoiseCancellation, onPropertiesUpdated['calls_'][1]![0]);
        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ true);

        const ncSupportedEnabled: crosAudioConfigMojom.AudioDevice =
            fakeCrosAudioConfig.createAudioDevice(
                fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ true);
        ncSupportedEnabled.noiseCancellationState =
            crosAudioConfigMojom.AudioEffectState.kEnabled;

        const enabledNoiseCancellation:
            crosAudioConfigMojom.AudioSystemProperties = {
          ...defaultNoiseCancellation,
          inputDevices: [ncSupportedEnabled],
        };
        assertDeepEquals(
            enabledNoiseCancellation, onPropertiesUpdated['calls_'][2]![0]);

        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ false);
        assertDeepEquals(
            defaultNoiseCancellation, onPropertiesUpdated['calls_'][2]![0]);
      });

  test(
      'VerifySetNoiseCancellationEnabledDoesNotUpdateUnsupportedDevice', () => {
        const ncNotSupported: crosAudioConfigMojom.AudioDevice =
            fakeCrosAudioConfig.createAudioDevice(
                fakeCrosAudioConfig.fakeInternalFrontMic, /*isActive=*/ true);
        ncNotSupported.noiseCancellationState =
            crosAudioConfigMojom.AudioEffectState.kNotSupported;

        const noNoiseCancellation:
            crosAudioConfigMojom.AudioSystemProperties = {
          ...defaultProperties,
          inputDevices: [ncNotSupported],
        };
        crosAudioConfig.setAudioSystemProperties(noNoiseCancellation);
        assertDeepEquals(
            noNoiseCancellation, onPropertiesUpdated['calls_'][1]![0]);
        const expectedCallCount = 2;
        assertEquals(expectedCallCount, onPropertiesUpdated['calls_'].length);
        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ true);
        crosAudioConfig.setNoiseCancellationEnabled(/*enabled=*/ false);
        assertEquals(expectedCallCount, onPropertiesUpdated['calls_'].length);
      });
});
