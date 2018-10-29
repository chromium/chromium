// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page', function() {
  suite('ProfileInfoTests', function() {
    /** @type {SettingsPeoplePageElement} */
    let peoplePage = null;
    /** @type {settings.ProfileInfoBrowserProxy} */
    let browserProxy = null;
    /** @type {settings.SyncBrowserProxy} */
    let syncBrowserProxy = null;

    suiteSetup(function() {
      loadTimeData.overrideValues({
        // Force easy unlock off. Those have their own ChromeOS-only tests.
        easyUnlockAllowed: false,
        // Force Dice off. Dice is tested in the DiceUITest suite.
        diceEnabled: false,
        // Force Unified Consent off. Unified Consent is tested in the
        // UnifiedConsentUITest suite.
        unifiedConsentEnabled: false,
      });
    });

    setup(function() {
      browserProxy = new TestProfileInfoBrowserProxy();
      settings.ProfileInfoBrowserProxyImpl.instance_ = browserProxy;

      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('settings-people-page');
      document.body.appendChild(peoplePage);

      return Promise
          .all([
            browserProxy.whenCalled('getProfileInfo'),
            syncBrowserProxy.whenCalled('getSyncStatus')
          ])
          .then(function() {
            Polymer.dom.flush();
          });
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('GetProfileInfo', function() {
      assertEquals(
          browserProxy.fakeProfileInfo.name,
          peoplePage.$$('#profile-name').textContent.trim());
      const bg = peoplePage.$$('#profile-icon').style.backgroundImage;
      assertTrue(bg.includes(browserProxy.fakeProfileInfo.iconUrl));

      const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
          'LAAAAAABAAEAAAICTAEAOw==';
      cr.webUIListenerCallback(
          'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

      Polymer.dom.flush();
      assertEquals(
          'pushedName', peoplePage.$$('#profile-name').textContent.trim());
      const newBg = peoplePage.$$('#profile-icon').style.backgroundImage;
      assertTrue(newBg.includes(iconDataUrl));
    });

    // This test ensures when unifiedConsentEnabled and diceEnabled is false,
    // the #sync-status row is shown instead of the #sync-setup row.
    test('ShowCorrectSyncRow', function() {
      sync_test_util.simulateSyncStatus({
        signedIn: true,
        syncSystemEnabled: true,
      });
      assertFalse(!!peoplePage.$$('#sync-setup'));
      assertTrue(!!peoplePage.$$('#sync-status'));
    });
  });

  if (!cr.isChromeOS) {
    suite('SyncStatusTests', function() {
      /** @type {SettingsPeoplePageElement} */
      let peoplePage = null;
      /** @type {settings.SyncBrowserProxy} */
      let browserProxy = null;
      /** @type {settings.ProfileInfoBrowserProxy} */
      let profileInfoBrowserProxy = null;

      suiteSetup(function() {
        // Force easy unlock off. Those have their own ChromeOS-only tests.
        loadTimeData.overrideValues({
          easyUnlockAllowed: false,
        });
      });

      setup(function() {
        browserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = browserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        document.body.appendChild(peoplePage);
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('Toast', function() {
        assertFalse(peoplePage.$.toast.open);
        cr.webUIListenerCallback('sync-settings-saved');
        assertTrue(peoplePage.$.toast.open);
      });

      // This makes sure UI meant for DICE-enabled profiles are not leaked to
      // non-dice profiles.
      // TODO(scottchen): This should be removed once all profiles are fully
      // migrated.
      test('NoManageProfileRow', function() {
        assertFalse(!!peoplePage.$$('#edit-profile'));
      });

      test('GetProfileInfo', function() {
        let disconnectButton = null;
        return browserProxy.whenCalled('getSyncStatus')
            .then(function() {
              Polymer.dom.flush();
              disconnectButton = peoplePage.$$('#disconnectButton');
              assertTrue(!!disconnectButton);
              assertFalse(!!peoplePage.$$('settings-signout-dialog'));

              disconnectButton.click();
              Polymer.dom.flush();
            })
            .then(function() {
              const signoutDialog = peoplePage.$$('settings-signout-dialog');
              assertTrue(signoutDialog.$$('#dialog').open);
              assertFalse(signoutDialog.$$('#deleteProfile').hidden);

              const deleteProfileCheckbox = signoutDialog.$$('#deleteProfile');
              assertTrue(!!deleteProfileCheckbox);
              assertLT(0, deleteProfileCheckbox.clientHeight);

              const disconnectConfirm = signoutDialog.$$('#disconnectConfirm');
              assertTrue(!!disconnectConfirm);
              assertFalse(disconnectConfirm.hidden);

              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              disconnectConfirm.click();

              return popstatePromise;
            })
            .then(function() {
              return browserProxy.whenCalled('signOut');
            })
            .then(function(deleteProfile) {
              assertFalse(deleteProfile);

              sync_test_util.simulateSyncStatus({
                signedIn: true,
                domain: 'example.com',
              });

              assertFalse(!!peoplePage.$$('#dialog'));
              disconnectButton.click();
              Polymer.dom.flush();

              return new Promise(function(resolve) {
                peoplePage.async(resolve);
              });
            })
            .then(function() {
              const signoutDialog = peoplePage.$$('settings-signout-dialog');
              assertTrue(signoutDialog.$$('#dialog').open);
              assertFalse(!!signoutDialog.$$('#deleteProfile'));

              const disconnectManagedProfileConfirm =
                  signoutDialog.$$('#disconnectManagedProfileConfirm');
              assertTrue(!!disconnectManagedProfileConfirm);
              assertFalse(disconnectManagedProfileConfirm.hidden);

              browserProxy.resetResolver('signOut');

              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              disconnectManagedProfileConfirm.click();

              return popstatePromise;
            })
            .then(function() {
              return browserProxy.whenCalled('signOut');
            })
            .then(function(deleteProfile) {
              assertTrue(deleteProfile);
            });
      });

      test('getProfileStatsCount', function() {
        return browserProxy.whenCalled('getSyncStatus')
            .then(function() {
              Polymer.dom.flush();

              // Open the disconnect dialog.
              disconnectButton = peoplePage.$$('#disconnectButton');
              assertTrue(!!disconnectButton);
              disconnectButton.click();

              return profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
            })
            .then(function() {
              Polymer.dom.flush();
              const signoutDialog = peoplePage.$$('settings-signout-dialog');
              assertTrue(signoutDialog.$$('#dialog').open);

              // Assert the warning message is as expected.
              const warningMessage =
                  signoutDialog.$$('.delete-profile-warning');

              cr.webUIListenerCallback('profile-stats-count-ready', 0);
              assertEquals(
                  loadTimeData.getStringF(
                      'deleteProfileWarningWithoutCounts', 'fakeUsername'),
                  warningMessage.textContent.trim());

              cr.webUIListenerCallback('profile-stats-count-ready', 1);
              assertEquals(
                  loadTimeData.getStringF(
                      'deleteProfileWarningWithCountsSingular', 'fakeUsername'),
                  warningMessage.textContent.trim());

              cr.webUIListenerCallback('profile-stats-count-ready', 2);
              assertEquals(
                  loadTimeData.getStringF(
                      'deleteProfileWarningWithCountsPlural', 2,
                      'fakeUsername'),
                  warningMessage.textContent.trim());

              // Close the disconnect dialog.
              signoutDialog.$$('#disconnectConfirm').click();
              return new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });
            });
      });

      test('NavigateDirectlyToSignOutURL', function() {
        // Navigate to chrome://settings/signOut
        settings.navigateTo(settings.routes.SIGN_OUT);

        return new Promise(function(resolve) {
                 peoplePage.async(resolve);
               })
            .then(function() {
              assertTrue(
                  peoplePage.$$('settings-signout-dialog').$$('#dialog').open);
              return profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
            })
            .then(function() {
              // 'getProfileStatsCount' can be the first message sent to the
              // handler if the user navigates directly to
              // chrome://settings/signOut. if so, it should not cause a crash.
              new settings.ProfileInfoBrowserProxyImpl().getProfileStatsCount();

              // Close the disconnect dialog.
              peoplePage.$$('settings-signout-dialog')
                  .$$('#disconnectConfirm')
                  .click();
            })
            .then(function() {
              return new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });
            });
      });

      test('Signout dialog suppressed when not signed in', function() {
        return browserProxy.whenCalled('getSyncStatus')
            .then(function() {
              settings.navigateTo(settings.routes.SIGN_OUT);
              return new Promise(function(resolve) {
                peoplePage.async(resolve);
              });
            })
            .then(function() {
              assertTrue(
                  peoplePage.$$('settings-signout-dialog').$$('#dialog').open);

              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              sync_test_util.simulateSyncStatus({
                signedIn: false,
              });

              return popstatePromise;
            })
            .then(function() {
              const popstatePromise = new Promise(function(resolve) {
                listenOnce(window, 'popstate', resolve);
              });

              settings.navigateTo(settings.routes.SIGN_OUT);

              return popstatePromise;
            });
      });

      test('syncStatusNotActionableForManagedAccounts', function() {
        assertFalse(!!peoplePage.$$('#sync-status'));

        return browserProxy.whenCalled('getSyncStatus').then(function() {
          sync_test_util.simulateSyncStatus({
            signedIn: true,
            syncSystemEnabled: true,
          });
          Polymer.dom.flush();

          let syncStatusContainer = peoplePage.$$('#sync-status');
          assertTrue(!!syncStatusContainer);
          assertTrue(syncStatusContainer.hasAttribute('actionable'));

          sync_test_util.simulateSyncStatus({
            managed: true,
            signedIn: true,
            syncSystemEnabled: true,
          });
          Polymer.dom.flush();

          syncStatusContainer = peoplePage.$$('#sync-status');
          assertTrue(!!syncStatusContainer);
          assertFalse(syncStatusContainer.hasAttribute('actionable'));
        });
      });

      test('syncStatusNotActionableForPassiveErrors', function() {
        assertFalse(!!peoplePage.$$('#sync-status'));

        return browserProxy.whenCalled('getSyncStatus').then(function() {
          sync_test_util.simulateSyncStatus({
            hasError: true,
            statusAction: settings.StatusAction.NO_ACTION,
            signedIn: true,
            syncSystemEnabled: true,
          });
          Polymer.dom.flush();

          let syncStatusContainer = peoplePage.$$('#sync-status');
          assertTrue(!!syncStatusContainer);
          assertFalse(syncStatusContainer.hasAttribute('actionable'));

          sync_test_util.simulateSyncStatus({
            hasError: true,
            statusAction: settings.StatusAction.UPGRADE_CLIENT,
            signedIn: true,
            syncSystemEnabled: true,
          });
          Polymer.dom.flush();

          syncStatusContainer = peoplePage.$$('#sync-status');
          assertTrue(!!syncStatusContainer);
          assertTrue(syncStatusContainer.hasAttribute('actionable'));
        });
      });
    });

    suite('DiceUITest', function() {
      /** @type {SettingsPeoplePageElement} */
      let peoplePage = null;
      /** @type {settings.SyncBrowserProxy} */
      let browserProxy = null;
      /** @type {settings.ProfileInfoBrowserProxy} */
      let profileInfoBrowserProxy = null;

      suiteSetup(function() {
        // Force UIs to think DICE is enabled for this profile.
        loadTimeData.overrideValues({
          diceEnabled: true,
        });
      });

      setup(function() {
        browserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = browserProxy;

        profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
        settings.ProfileInfoBrowserProxyImpl.instance_ =
            profileInfoBrowserProxy;

        PolymerTest.clearBody();
        peoplePage = document.createElement('settings-people-page');
        document.body.appendChild(peoplePage);

        Polymer.dom.flush();
      });

      teardown(function() {
        peoplePage.remove();
      });

      test('ShowCorrectRows', function() {
        return browserProxy.whenCalled('getSyncStatus').then(function() {
          // The correct /manageProfile link row is shown.
          assertTrue(!!peoplePage.$$('#edit-profile'));
          assertFalse(!!peoplePage.$$('#picture-subpage-trigger'));

          // Sync-overview row should not exist when diceEnabled is true, even
          // if syncStatus values would've warranted the row otherwise.
          sync_test_util.simulateSyncStatus({
            signedIn: false,
            signinAllowed: true,
            syncSystemEnabled: true,
          });
          assertFalse(!!peoplePage.$$('#sync-overview'));

          // The control element should exist when policy allows.
          const accountControl = peoplePage.$$('settings-sync-account-control');
          assertTrue(
              window.getComputedStyle(accountControl)['display'] != 'none');

          // Control element doesn't exist when policy forbids sync or sign-in.
          sync_test_util.simulateSyncStatus({
            signinAllowed: false,
            syncSystemEnabled: true,
          });
          assertEquals(
              window.getComputedStyle(accountControl)['display'], 'none');

          sync_test_util.simulateSyncStatus({
            signinAllowed: true,
            syncSystemEnabled: false,
          });
          assertEquals(
              window.getComputedStyle(accountControl)['display'], 'none');
        });
      });

      // This test ensures when diceEnabled is true, but unifiedConsentEnabled
      // is false, the #sync-status row is shown instead of the #sync-setup row.
      test('ShowCorrectSyncRowWithDice', function() {
        sync_test_util.simulateSyncStatus({
          signedIn: true,
          syncSystemEnabled: true,
        });
        assertFalse(!!peoplePage.$$('#sync-setup'));
        assertTrue(!!peoplePage.$$('#sync-status'));
      });
    });
  }

  suite('UnifiedConsentUITest', function() {
    /** @type {SettingsPeoplePageElement} */
    let peoplePage = null;
    /** @type {settings.SyncBrowserProxy} */
    let browserProxy = null;
    /** @type {settings.ProfileInfoBrowserProxy} */
    let profileInfoBrowserProxy = null;

    suiteSetup(function() {
      // Force UIs to think DICE is enabled for this profile.
      loadTimeData.overrideValues({
        diceEnabled: true,
        unifiedConsentEnabled: true,
      });
    });

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
      settings.ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('settings-people-page');
      document.body.appendChild(peoplePage);

      Polymer.dom.flush();
      return browserProxy.whenCalled('getSyncStatus');
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('ShowCorrectSyncRowWithUnifiedConsent', function() {
      assertTrue(!!peoplePage.$$('#sync-setup'));
      assertFalse(!!peoplePage.$$('#sync-status'));

      // Make sures the subpage opens even when logged out or has errors.
      sync_test_util.simulateSyncStatus({
        signedIn: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
      });

      peoplePage.$$('#sync-setup').click();
      Polymer.dom.flush();

      assertEquals(settings.getCurrentRoute(), settings.routes.SYNC);
    });
  });
});
