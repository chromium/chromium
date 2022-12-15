// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {crosAudioConfigMojomWebui, fakeCrosAudioConfig} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertDeepEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {MockController, MockMethod} from 'chrome://webui-test/mock_controller.js';

suite('FakeCrosAudioConfig', function() {
  // Observer for testing updates which have not been added to mojo yet.
  // Implements `fakeCrosAudioConfig.FakePropertiesObserverInterface`.
  class TestPropertiesObserver {
    onPropertiesUpdated(properties) {}
  }

  /** @type {crosAudioConfigMojomWebui.AudioSystemProperties} */
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

    /** @type {crosAudioConfigMojomWebui.fakeObserver} */
    const observer = new TestPropertiesObserver();
    /** @type {MockMethod} */
    const onObserverPropertiesUpdated =
        mockController.createFunctionMock(observer, 'onPropertiesUpdated');

    assertEquals(0, onObserverPropertiesUpdated.calls_.length);
    crosAudioConfig.observeAudioSystemProperties(observer);

    assertDeepEquals(
        defaultProperties, onObserverPropertiesUpdated.calls_[0][0]);
    assertDeepEquals(defaultProperties, onPropertiesUpdated.calls_[1][0]);

    /** @type {crosAudioConfigMojomWebui.AudioSystemProperties} */
    const updatedProperties = {
      outputDevices: [fakeCrosAudioConfig.fakeSpeakerActive],
      outputVolumePercent: 0,
      outputMuteState: crosAudioConfigMojomWebui.MuteState.kMutedByUser,
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
        crosAudioConfig.setActiveDevice(fakeCrosAudioConfig.defaultFakeSpeaker);

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
    crosAudioConfig.setActiveDevice(fakeCrosAudioConfig.fakeBluetoothMic);

    mockController.verifyMocks();
  });

  test('VerifySetOutputMutedTriggersMatchingPropertyUpdate', () => {
    /** @type {AudioSystemProperties} */
    const propertiesOutputMutedByUser = {
      ...defaultProperties,
      outputMuteState: crosAudioConfigMojomWebui.MuteState.kMutedByUser,
    };
    onPropertiesUpdated.addExpectation(propertiesOutputMutedByUser);
    crosAudioConfig.setOutputMuted(/*muted=*/ true);
    assertDeepEquals(
        propertiesOutputMutedByUser, onPropertiesUpdated.calls_[1][0]);

    /** @type {AudioSystemProperties} */
    const propertiesOutputUnmute = {
      ...defaultProperties,
      outputMuteState: crosAudioConfigMojomWebui.MuteState.kNotMuted,
    };
    onPropertiesUpdated.addExpectation(propertiesOutputUnmute);
    crosAudioConfig.setOutputMuted(/*muted=*/ false);
    assertDeepEquals(propertiesOutputUnmute, onPropertiesUpdated.calls_[2][0]);
  });

  test('VerifySetInputMutedTriggersMatchingPropertyUpdate', () => {
    assertEquals(
        crosAudioConfigMojomWebui.MuteState.kNotMuted,
        onPropertiesUpdated.calls_[0][0].inputMuteState);

    /** @type {AudioSystemProperties} */
    const updateInputGainMuted = {
      ...defaultProperties,
      inputMuteState: crosAudioConfigMojomWebui.MuteState.kMutedByUser,
    };
    onPropertiesUpdated.addExpectation(updateInputGainMuted);
    crosAudioConfig.setInputMuted(/*muted=*/ true);

    assertEquals(
        crosAudioConfigMojomWebui.MuteState.kMutedByUser,
        onPropertiesUpdated.calls_[1][0].inputMuteState);
    onPropertiesUpdated.addExpectation(defaultProperties);
    crosAudioConfig.setInputMuted(/*muted=*/ false);

    assertEquals(
        crosAudioConfigMojomWebui.MuteState.kNotMuted,
        onPropertiesUpdated.calls_[2][0].inputMuteState);
  });

  test('VerifySetInputVolumeTriggersMatchingPropertyUpdate', () => {
    const expectedVolumePercent = 32;
    /** @type {AudioSystemProperties} */
    const updatedProperties = {
      ...defaultProperties,
      inputVolumePercent: expectedVolumePercent,
    };
    onPropertiesUpdated.addExpectation(updatedProperties);
    crosAudioConfig.setInputVolumePercent(expectedVolumePercent);

    mockController.verifyMocks();
  });
});
