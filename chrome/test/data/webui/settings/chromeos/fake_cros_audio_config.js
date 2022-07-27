// Copyright 2022 The Chromium Authors. All rights reserved.
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

/** @type {!ash.audioConfig.mojom.AudioSystemProperties} */
export const defaultFakeAudioSystemProperties = {
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
   * Sets the outputVolumePercent to the desired volume.
   * @param {!ash.audioConfig.mojom.AudioSystemProperties} properties
   */
  setAudioSystemProperties(properties) {
    this.audioSystemProperties_ = properties;
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
