// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.AndroidAppsBrowserProxy} */
class TestAndroidAppsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestAndroidAppsInfo',
      'showAndroidAppsSettings',
    ]);
  }

  /** @override */
  requestAndroidAppsInfo() {
    this.methodCalled('requestAndroidAppsInfo');
    this.setAndroidAppsState(false, false);
  }

  /** override */
  showAndroidAppsSettings(keyboardAction) {
    this.methodCalled('showAndroidAppsSettings');
  }

  setAndroidAppsState(playStoreEnabled, settingsAppAvailable) {
    // We need to make sure to pass a new object here, otherwise the property
    // change event may not get fired in the listener.
    const appsInfo = {
      playStoreEnabled: playStoreEnabled,
      settingsAppAvailable: settingsAppAvailable,
    };
    cr.webUIListenerCallback('android-apps-info-update', appsInfo);
  }
}
