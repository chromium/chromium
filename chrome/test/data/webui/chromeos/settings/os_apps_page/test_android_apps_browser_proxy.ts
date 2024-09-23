// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AndroidAppsBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAndroidAppsBrowserProxy extends TestBrowserProxy implements
    AndroidAppsBrowserProxy {
  constructor() {
    super([
      'requestAndroidAppsInfo',
      'showAndroidAppsSettings',
      'showPlayStoreApps',
    ]);
  }

  requestAndroidAppsInfo(): void {
    this.methodCalled('requestAndroidAppsInfo');
    this.setAndroidAppsState(false, false);
  }

  showAndroidAppsSettings(keyboardAction: boolean): void {
    this.methodCalled('showAndroidAppsSettings', keyboardAction);
  }

  openGooglePlayStore(url: string): void {
    this.methodCalled('showPlayStoreApps', url);
  }

  setAndroidAppsState(playStoreEnabled: boolean, settingsAppAvailable: boolean):
      void {
    // We need to make sure to pass a new object here, otherwise the property
    // change event may not get fired in the listener.
    const appsInfo = {
      playStoreEnabled,
      settingsAppAvailable,
    };
    webUIListenerCallback('android-apps-info-update', appsInfo);
  }
}
