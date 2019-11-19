// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of accessibility tests for the SIGN_OUT route.
 * Not used on Chrome OS since signing out is done at the OS level, not within
 * the Chrome Browser.
 */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

/**
 * Test fixture for SIGN_OUT
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsA11ySignOut() {}

SettingsA11ySignOut.prototype = {
  __proto__: SettingsAccessibilityTest.prototype,

  // Include files that define the mocha tests.
  extraLibraries: SettingsAccessibilityTest.prototype.extraLibraries.concat([
    '../../test_browser_proxy.js',
    '../../test_util.js',
    '../sync_test_util.js',
    '../test_sync_browser_proxy.js',
  ]),
};

AccessibilityTest.define('SettingsA11ySignOut', {
  /** @override */
  name: 'SIGN_OUT',
  /** @type {SettingsPeoplePageElement}*/
  peoplePage: null,
  /** @type {TestSyncBrowserProxy}*/
  browserProxy: null,
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    // Reset the blank to be a blank page.
    PolymerTest.clearBody();

    // Set the URL of the page to render to load the correct view upon
    // injecting settings-ui without attaching listeners.
    window.history.pushState(
        'object or string', 'Test', settings.routes.PEOPLE.path);

    this.browserProxy = new TestSyncBrowserProxy();
    settings.SyncBrowserProxyImpl.instance_ = this.browserProxy;

    const settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);
    Polymer.dom.flush();

    this.peoplePage = settingsUi.$$('settings-main')
                          .$$('settings-basic-page')
                          .$$('settings-people-page');
    assert(!!this.peoplePage);

    if (this.peoplePage.diceEnabled_) {
      sync_test_util.simulateSyncStatus({
        signedIn: false,
        signinAllowed: true,
        syncSystemEnabled: true,
        disabled: false,
      });
    }
  },
  /** @override */
  tests: {
    'Accessible Dialog': function() {
      let parent = null;
      let disconnectButtonSelector = null;

      return this.browserProxy.getSyncStatus()
          .then(syncStatus => {
            // Navigate to the sign out dialog.
            Polymer.dom.flush();

            if (this.peoplePage.diceEnabled_) {
              const syncAccountControl =
                  this.peoplePage.$$('settings-sync-account-control');
              syncAccountControl.syncStatus = {
                firstSetupInProgress: false,
                signedIn: true,
                signedInUsername: 'bar@bar.com',
                statusAction: settings.StatusAction.NO_ACTION,
                hasError: false,
                disabled: false,
              };
              parent = syncAccountControl;
              disconnectButtonSelector = '#turn-off';
            } else {
              parent = this.peoplePage;
              disconnectButtonSelector = '#disconnectButton';
            }
            return test_util.waitBeforeNextRender(parent);
          })
          .then(() => {
            disconnectButton = parent.$$(disconnectButtonSelector);
            assert(!!disconnectButton);
            disconnectButton.click();
            Polymer.dom.flush();
          });
    }
  },
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
