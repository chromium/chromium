// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrivacyHubBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {GeolocationAccessLevel} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacyHubBrowserProxy extends TestBrowserProxy implements
    PrivacyHubBrowserProxy {
  microphoneToggleIsEnabled: boolean;
  microphoneMutedBySecurityCurtain: boolean;
  cameraSwitchIsForceDisabled: boolean;
  primaryUserLocationAccessLevel: GeolocationAccessLevel;
  cameraLEDFallbackState: boolean;
  currentTimeZoneName: string;
  currentSunRiseTime: string;
  currentSunSetTime: string;
  constructor() {
    super([
      'getInitialMicrophoneHardwareToggleState',
      'getInitialMicrophoneMutedBySecurityCurtainState',
      'getInitialCameraSwitchForceDisabledState',
      'getInitialGeolocationAccessLevelState',
      'getCameraLedFallbackState',
      'getCurrentTimeZoneName',
      'getCurrentSunriseTime',
      'getCurrentSunsetTime',
    ]);
    this.microphoneToggleIsEnabled = false;
    this.microphoneMutedBySecurityCurtain = false;
    this.cameraSwitchIsForceDisabled = false;
    this.primaryUserLocationAccessLevel = GeolocationAccessLevel.DISALLOWED;
    this.cameraLEDFallbackState = false;
    this.currentTimeZoneName = 'Test Time Zone';
    this.currentSunRiseTime = '7:00AM';
    this.currentSunSetTime = '8:00PM';
  }

  getInitialMicrophoneHardwareToggleState(): Promise<boolean> {
    this.methodCalled('getInitialMicrophoneHardwareToggleState');
    return Promise.resolve(this.microphoneToggleIsEnabled);
  }

  getInitialMicrophoneMutedBySecurityCurtainState(): Promise<boolean> {
    this.methodCalled('getInitialMicrophoneMutedBySecurityCurtainState');
    return Promise.resolve(this.microphoneMutedBySecurityCurtain);
  }

  getInitialCameraSwitchForceDisabledState(): Promise<boolean> {
    this.methodCalled('getInitialCameraSwitchForceDisabledState');
    return Promise.resolve(this.cameraSwitchIsForceDisabled);
  }

  getInitialGeolocationAccessLevelState(): Promise<GeolocationAccessLevel> {
    this.methodCalled('getInitialGeolocationAccessLevelState');
    return Promise.resolve(this.primaryUserLocationAccessLevel);
  }

  getCameraLedFallbackState(): Promise<boolean> {
    this.methodCalled('getCameraLedFallbackState');
    return Promise.resolve(this.cameraLEDFallbackState);
  }

  getCurrentTimeZoneName(): Promise<string> {
    this.methodCalled('getCurrentTimeZoneName');
    return Promise.resolve(this.currentTimeZoneName);
  }

  getCurrentSunriseTime(): Promise<string> {
    this.methodCalled('getCurrentSunriseTime');
    return Promise.resolve(this.currentSunRiseTime);
  }

  getCurrentSunsetTime(): Promise<string> {
    this.methodCalled('getCurrentSunsetTime');
    return Promise.resolve(this.currentSunSetTime);
  }
}
