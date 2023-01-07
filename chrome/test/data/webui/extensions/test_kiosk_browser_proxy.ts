// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KioskAppSettings, KioskBrowserProxy, KioskSettings} from 'chrome://extensions/extensions.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestKioskBrowserProxy extends TestBrowserProxy implements
    KioskBrowserProxy {
  private initialSettings_: KioskSettings;
  private appSettings_: KioskAppSettings;

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

    this.initialSettings_ = {
      kioskEnabled: true,
      autoLaunchEnabled: false,
    };

    this.appSettings_ = {
      apps: [],
      disableBailout: false,
      hasAutoLaunchApp: false,
    };
  }

  setAppSettings(settings: KioskAppSettings) {
    this.appSettings_ = settings;
  }

  setInitialSettings(settings: KioskSettings) {
    this.initialSettings_ = settings;
  }

  initializeKioskAppSettings() {
    this.methodCalled('initializeKioskAppSettings');
    return Promise.resolve(this.initialSettings_);
  }

  getKioskAppSettings() {
    this.methodCalled('getKioskAppSettings');
    return Promise.resolve(this.appSettings_);
  }

  addKioskApp(appId: string) {
    this.methodCalled('addKioskApp', appId);
  }

  disableKioskAutoLaunch(appId: string) {
    this.methodCalled('disableKioskAutoLaunch', appId);
  }

  enableKioskAutoLaunch(appId: string) {
    this.methodCalled('enableKioskAutoLaunch', appId);
  }

  removeKioskApp(appId: string) {
    this.methodCalled('removeKioskApp', appId);
  }

  setDisableBailoutShortcut(disableBailout: boolean) {
    this.methodCalled('setDisableBailoutShortcut', disableBailout);
  }
}
