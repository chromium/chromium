// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Fake implementation of CrosAudioConfig for testing.
 */

/**
 * Enumeration of all possible MuteState options for CrosAudioConfig.
 * Note: This must be kept in sync with CrosAudioConfig in
 * ash//components/audio/public/mojom/cros_audio_config.mojom
 * @enum {number}
 */
export const CrosAudioConfigMuteState = {
  kNotMuted: 0,
  kMutedByUser: 1,
  kMutedByPolicy: 2,
};

/**
 * Enumeration of all possible AudioDeviceType options for CrosAudioConfig.
 * Note: This must be kept in sync with CrosAudioConfig in
 * ash//components/audio/public/mojom/cros_audio_config.mojom
 * @enum {number}
 */
export const CrosAudioConfigAudioDeviceType = {
  kHeadphone: 0,
  kMic: 1,
  kUsb: 2,
  kBluetooth: 3,
  kBluetoothNbMic: 4,
  kHdmi: 5,
  kInternalSpeaker: 6,
  kInternalMic: 7,
  kFrontMic: 8,
  kRearMic: 9,
  kKeyboardMic: 10,
  kHotword: 11,
  kPostDspLoopback: 12,
  kPostMixLoopback: 13,
  kLineout: 14,
  kAlsaLoopback: 15,
  kOther: 16,
};

/** @type {!ash.audioConfig.mojom.AudioDevice} */
export const crosAudioConfigDefaultFakeMicJack = {
  /** @type {bigint} */
  id: BigInt(1),

  /** @type {string} */
  displayName: 'Mic Jack',

  /** @type {boolean} */
  isActive: true,

  /** @type {!ash.audioConfig.mojom.AudioDeviceType} */
  deviceType: CrosAudioConfigAudioDeviceType.kInternalMic,
};

/** @type {!ash.audioConfig.mojom.AudioDevice} */
export const crosAudioConfigActiveFakeSpeaker = {
  /** @type {bigint} */
  id: BigInt(2),

  /** @type {string} */
  displayName: 'Speaker',

  /** @type {boolean} */
  isActive: true,

  /** @type {!ash.audioConfig.mojom.AudioDeviceType} */
  deviceType: CrosAudioConfigAudioDeviceType.kInternalSpeaker,
};

/** @type {!ash.audioConfig.mojom.AudioDevice} */
export const crosAudioConfigInactiveFakeMicJack = {
  /** @type {bigint} */
  id: BigInt(3),

  /** @type {string} */
  displayName: 'Mic Jack',

  /** @type {boolean} */
  isActive: false,

  /** @type {!ash.audioConfig.mojom.AudioDeviceType} */
  deviceType: CrosAudioConfigAudioDeviceType.kInternalSpeaker,
};

/** @type {!ash.audioConfig.mojom.AudioDevice} */
export const crosAudioConfigDefaultFakeSpeaker = {
  /** @type {bigint} */
  id: BigInt(4),

  /** @type {string} */
  displayName: 'Speaker',

  /** @type {boolean} */
  isActive: false,

  /** @type {!ash.audioConfig.mojom.AudioDeviceType} */
  deviceType: CrosAudioConfigAudioDeviceType.kInternalSpeaker,
};

/** @type {!ash.audioConfig.mojom.AudioSystemProperties} */
export const defaultFakeAudioSystemProperties = {
  /** @type {!Array<!ash.audioConfig.mojom.AudioDevice>} */
  outputDevices:
      [crosAudioConfigDefaultFakeSpeaker, crosAudioConfigDefaultFakeMicJack],

  outputVolumePercent: 75,

  /** @type {!ash.audioConfig.mojom.MuteState} */
  outputMuteState: CrosAudioConfigMuteState.kNotMuted,
};

/** @implements {ash.audioConfig.mojom.CrosAudioConfigInterface} */
export class FakeCrosAudioConfig {
  constructor() {
    /** @private {!ash.audioConfig.mojom.AudioSystemProperties} */
    this.audioSystemProperties_ = defaultFakeAudioSystemProperties;

    /**
     * @private {!Array<!ash.audioConfig.mojom.AudioSystemPropertiesObserverInterface>}
     */
    this.audio_system_properties_observers_ = [];
  }

  /**
   * @override
   * @param {!ash.audioConfig.mojom.AudioSystemPropertiesObserverInterface}
   *     observer
   */
  observeAudioSystemProperties(observer) {
    this.audio_system_properties_observers_.push(observer);
    this.notifyAudioSystemPropertiesUpdated_();
  }

  /**
   * Sets audioSystemProperties to the desired properties.
   * @param {!ash.audioConfig.mojom.AudioSystemProperties} properties
   */
  setAudioSystemProperties(properties) {
    this.audioSystemProperties_ = properties;
    this.notifyAudioSystemPropertiesUpdated_();
  }

  /**
   * Sets the outputVolumePercent to the desired volume.
   * @param {!number} volume
   */
  setOutputVolumePercent(volume) {
    this.audioSystemProperties_.outputVolumePercent = volume;
    this.notifyAudioSystemPropertiesUpdated_();
  }

  /**
   * @private
   * Notifies the observer list that audioSystemProperties_ has changed.
   */
  notifyAudioSystemPropertiesUpdated_() {
    this.audio_system_properties_observers_.forEach((observer) => {
      observer.onPropertiesUpdated(this.audioSystemProperties_);
    });
  }
}
