// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of NearbyShareSettings for testing.
 */

import {DataUsage, DeviceNameValidationResult, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/**
 * Fake implementation of NearbyShareSettingsInterface
 */
export class FakeNearbyShareSettings {
  constructor() {
    /** @private {!boolean} */
    this.enabled_ = true;
    /** @private {!FastInitiationNotificationState} */
    this.fastInitiationNotificationState_ =
        FastInitiationNotificationState.kEnabled;
    /** @private {!boolean} */
    this.isFastInitiationHardwareSupported_ = true;
    /** @private {!string} */
    this.deviceName_ = 'testDevice';
    /** @private {!DataUsage} */
    this.dataUsage_ = DataUsage.kOnline;
    /** @private {!Visibility} */
    this.visibility_ = Visibility.kAllContacts;
    /** @private {!Array<!string>} */
    this.allowedContacts_ = [];
    /**
     * Restore NearbyShareSettingsObserverInterface type when migrated to TS.
     * @private {!Object}
     */
    this.observer_;
    /** @private {!DeviceNameValidationResult} */
    this.nextDeviceNameResult_ = DeviceNameValidationResult.kValid;
    /** @private {!boolean} */
    this.isOnboardingComplete_ = false;
    /** @private {Object} */
    this.$ = {
      close() {},
    };
  }

  /**
   * Restore NearbyShareSettingsObserverInterface type when migrated to TS.
   * @param { !Object } observer
   */
  addSettingsObserver(observer) {
    // Just support a single observer for testing.
    this.observer_ = observer;
  }

  /**
   * @param { !DeviceNameValidationResult } result
   */
  setNextDeviceNameResult(result) {
    // Set the next result to be used when calling ValidateDeviceName() or
    // SetDeviceName().
    this.nextDeviceNameResult_ = result;
  }

  /**
   * @return {!Promise<{enabled: !boolean}>}
   */
  async getEnabled() {
    return {enabled: this.enabled_};
  }

  /**
   * @return {!Promise<{supported: !boolean}>}
   */
  async getIsFastInitiationHardwareSupported() {
    return {supported: this.isFastInitiationHardwareSupported_};
  }

  /**
   * @return {!Promise<{state:
   *     !FastInitiationNotificationState}>}
   */
  async getFastInitiationNotificationState() {
    return {state: this.fastInitiationNotificationState_};
  }

  /**
   * @param { !boolean } enabled
   */
  setEnabled(enabled) {
    this.enabled_ = enabled;
    if (this.observer_) {
      this.observer_.onEnabledChanged(enabled);
    }
  }

  /**
   * @param { !FastInitiationNotificationState } state
   */
  setFastInitiationNotificationState(state) {
    this.fastInitiationNotificationState_ = state;
    if (this.observer_) {
      this.observer_.onFastInitiationNotificationStateChanged(state);
    }
  }

  /**
   * @return {!Promise<{completed: !boolean}>}
   */
  async isOnboardingComplete() {
    return {completed: this.isOnboardingComplete_};
  }

  /**
   * @return {!Promise<{deviceName: !string}>}
   */
  async getDeviceName() {
    return {deviceName: this.deviceName_};
  }

  /**
   * @param { !string } deviceName
   * @return {!Promise<{
        result: !DeviceNameValidationResult,
   *  }>}
   */
  async validateDeviceName(deviceName) {
    return {result: this.nextDeviceNameResult_};
  }

  /**
   * @param { !string } deviceName
   * @return {!Promise<{
        result: !DeviceNameValidationResult,
   *  }>}
   */
  async setDeviceName(deviceName) {
    if (this.nextDeviceNameResult_ !== DeviceNameValidationResult.kValid) {
      return {result: this.nextDeviceNameResult_};
    }

    this.deviceName_ = deviceName;
    if (this.observer_) {
      this.observer_.onDeviceNameChanged(deviceName);
    }

    return {result: this.nextDeviceNameResult_};
  }

  /**
   * @return {!Promise<{dataUsage: !DataUsage}>}
   */
  async getDataUsage() {
    return {dataUsage: this.dataUsage_};
  }

  /**
   * @param { !DataUsage } dataUsage
   */
  setDataUsage(dataUsage) {
    this.dataUsage_ = dataUsage;
    if (this.observer_) {
      this.observer_.onDataUsageChanged(dataUsage);
    }
  }

  /**
   * @return {!Promise<{visibility: !Visibility}>}
   */
  async getVisibility() {
    return {visibility: this.visibility_};
  }

  /**
   * @param { !Visibility } visibility
   */
  setVisibility(visibility) {
    this.visibility_ = visibility;
    if (this.observer_) {
      this.observer_.onVisibilityChanged(visibility);
    }
  }

  /**
   * @return {!Promise<{allowedContacts: !Array<!string>}>}
   */
  async getAllowedContacts() {
    return {allowedContacts: this.allowedContacts_};
  }

  /**
   * @param { !Array<!string> } allowedContacts
   */

  setAllowedContacts(allowedContacts) {
    this.allowedContacts_ = allowedContacts;
    if (this.observer_) {
      this.observer_.onAllowedContactsChanged(this.allowedContacts_);
    }
  }

  /**
   * @param { !boolean } supported
   */
  setIsFastInitiationHardwareSupportedForTest(supported) {
    this.isFastInitiationHardwareSupported_ = supported;
  }

  /**
   * @param { !boolean } completed
   */
  setIsOnboardingComplete(completed) {
    this.isOnboardingComplete_ = completed;
    if (this.observer_) {
      this.observer_.onIsOnboardingCompleteChanged(this.isOnboardingComplete_);
    }
  }

  getEnabledForTest() {
    return this.enabled_;
  }

  getIsFastInitiationHardwareSupportedTest() {
    return this.isFastInitiationHardwareSupported_;
  }

  getFastInitiationNotificationStateTest() {
    return this.fastInitiationNotificationState_;
  }

  getDeviceNameForTest() {
    return this.deviceName_;
  }

  getDataUsageForTest() {
    return this.dataUsage_;
  }

  getVisibilityForTest() {
    return this.visibility_;
  }

  getAllowedContactsForTest() {
    return this.allowedContacts_;
  }
}
