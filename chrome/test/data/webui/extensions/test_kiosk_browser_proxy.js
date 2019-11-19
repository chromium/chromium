// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {KioskBrowserProxy} */
export class TestKioskBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initializeKioskAppSettings',
      'getKioskAppSettings',
      'addKioskApp',
      'disableKioskAutoLaunch',
      'enableKioskAutoLaunch',
      'removeKioskApp',
      'setDisableBailoutShortcut',
    ]);

    /** @private {!KioskSettings} */
    this.initialSettings_ = {
      kioskEnabled: true,
      autoLaunchEnabled: false,
    };

    /** @private {!KioskAppSettings} */
    this.appSettings_ = {
      apps: [],
      disableBailout: false,
      hasAutoLaunchApp: false,
    };
  }

  /** @param {!KioskAppSettings} */
  setAppSettings(settings) {
    this.appSettings_ = settings;
  }

  /** @param {!KioskSettings} */
  setInitialSettings(settings) {
    this.initialSettings_ = settings;
  }

  /** @override */
  initializeKioskAppSettings() {
    this.methodCalled('initializeKioskAppSettings');
    return Promise.resolve(this.initialSettings_);
  }

  /** @override */
  getKioskAppSettings() {
    this.methodCalled('getKioskAppSettings');
    return Promise.resolve(this.appSettings_);
  }

  /** @override */
  addKioskApp(appId) {
    this.methodCalled('addKioskApp', appId);
  }

  /** @override */
  disableKioskAutoLaunch(appId) {
    this.methodCalled('disableKioskAutoLaunch', appId);
  }

  /** @override */
  enableKioskAutoLaunch(appId) {
    this.methodCalled('enableKioskAutoLaunch', appId);
  }

  /** @override */
  removeKioskApp(appId) {
    this.methodCalled('removeKioskApp', appId);
  }

  /** @override */
  setDisableBailoutShortcut(disableBailout) {
    this.methodCalled('setDisableBailoutShortcut', disableBailout);
  }
}