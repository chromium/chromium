// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {crosAudioConfigMojomWebui, fakeCrosAudioConfig} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertDeepEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {MockController, MockMethod} from 'chrome://webui-test/mock_controller.js';

suite('FakeCrosAudioConfig', function() {
  /** @type {crosAudioConfigMojomWebui.AudioSystemProperties} */
  const defaultProperties =
      fakeCrosAudioConfig.defaultFakeAudioSystemProperties;

  /** @type {?fakeCrosAudioConfig.FakeCrosAudioConfig} */
  let crosAudioConfig = null;
  /** @type {?crosAudioConfigMojomWebui.AudioSystemPropertiesObserverRemote} */
  let audioSystemPropertiesObserverRemote = null;

  /** @type {?MockController} */
  let mockController = null;
  /** @type {?MockMethod} */
  let onPropertiesUpdated = null;

  setup(() => {
    crosAudioConfig = new fakeCrosAudioConfig.FakeCrosAudioConfig();
    audioSystemPropertiesObserverRemote =
        new crosAudioConfigMojomWebui.AudioSystemPropertiesObserverRemote();
    mockController = new MockController();
    onPropertiesUpdated = mockController.createFunctionMock(
        audioSystemPropertiesObserverRemote, 'onPropertiesUpdated');
    onPropertiesUpdated.addExpectation(defaultProperties);
    crosAudioConfig.observeAudioSystemProperties(
        audioSystemPropertiesObserverRemote);
  });

  teardown(() => {
    crosAudioConfig = null;
    audioSystemPropertiesObserverRemote = null;
    mockController = null;
    onPropertiesUpdated = null;
  });

  test('VerifyObserversReceiveAudioSystemProperitesUpdates', () => {
    // `audioSystemPropertiesObserverRemote` observer initialized during setup.
    assertDeepEquals(defaultProperties, onPropertiesUpdated.calls_[0][0]);

    /** @type {crosAudioConfigMojomWebui.AudioSystemPropertiesObserverRemote} */
    const observer =
        new crosAudioConfigMojomWebui.AudioSystemPropertiesObserverRemote();
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

  test('VerifySetActiveDeviceTriggersMatchingPropertyUpdate', () => {
    /** @type {crosAudioConfigMojomWebui.AudioSystemProperties} */
    const updatedProperties = {
      ...defaultProperties,
      outputDevices: [
        fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.defaultFakeSpeaker, /*isActive=*/ true),
        fakeCrosAudioConfig.createAudioDevice(
            fakeCrosAudioConfig.defaultFakeMicJack, /*isActive=*/ false),
      ],
    };
    onPropertiesUpdated.addExpectation(updatedProperties);
    crosAudioConfig.setActiveDevice(fakeCrosAudioConfig.defaultFakeSpeaker);

    mockController.verifyMocks();
  });
});
