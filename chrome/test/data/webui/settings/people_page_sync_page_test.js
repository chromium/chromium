// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_sync_page', function() {

  suite('AdvancedSyncSettingsTests', function() {
    let syncPage = null;
    let browserProxy = null;
    let encryptWithGoogle = null;
    let encryptWithPassphrase = null;

    /**
     * Returns sync prefs with everything synced and no passphrase required.
     * @return {!settings.SyncPrefs}
     */
    function getSyncAllPrefs() {
      return {
        appsEnforced: false,
        appsRegistered: true,
        appsSynced: true,
        autofillEnforced: false,
        autofillRegistered: true,
        autofillSynced: true,
        bookmarksEnforced: false,
        bookmarksRegistered: true,
        bookmarksSynced: true,
        encryptAllData: false,
        encryptAllDataAllowed: true,
        enterGooglePassphraseBody: 'Enter Google passphrase.',
        enterPassphraseBody: 'Enter custom passphrase.',
        extensionsEnforced: false,
        extensionsRegistered: true,
        extensionsSynced: true,
        fullEncryptionBody: '',
        passphrase: '',
        passphraseRequired: false,
        passphraseTypeIsCustom: false,
        passwordsEnforced: false,
        passwordsRegistered: true,
        passwordsSynced: true,
        paymentsIntegrationEnabled: true,
        preferencesEnforced: false,
        preferencesRegistered: true,
        preferencesSynced: true,
        setNewPassphrase: false,
        syncAllDataTypes: true,
        tabsEnforced: false,
        tabsRegistered: true,
        tabsSynced: true,
        themesEnforced: false,
        themesRegistered: true,
        themesSynced: true,
        typedUrlsEnforced: false,
        typedUrlsRegistered: true,
        typedUrlsSynced: true,
        userEventsEnforced: false,
        userEventsRegistered: true,
        userEventsSynced: true,
      };
    }

    function openDatatypeConfigurationWithUnifiedConsent(prefs) {
      syncPage.unifiedConsentEnabled = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      Polymer.dom.flush();

      const syncAllDataTypesControl = syncPage.$.syncAllDataTypesControl;
      assertFalse(syncAllDataTypesControl.disabled);
      assertTrue(syncAllDataTypesControl.checked);

      // Uncheck the Sync All control.
      syncAllDataTypesControl.click();
    }

    // Tests the initial layout of the sync section and the personalize section,
    // depending on the sync state and the unified consent state.
    function testInitialLayout(
        unifiedConsentGiven, signedIn, hasError, setupInProgress,
        syncSectionExpanded, syncSectionDisabled, personalizeSectionExpanded) {
      syncPage.unifiedConsentEnabled = true;
      syncPage.prefs = {unified_consent_given: {value: unifiedConsentGiven}};
      syncPage.syncStatus = {
        signedIn: signedIn,
        disabled: false,
        hasError: hasError,
        setupInProgress: setupInProgress,
        statusAction: hasError ? settings.StatusAction.REAUTHENTICATE :
                                 settings.StatusAction.NO_ACTION,
      };
      Polymer.dom.flush();

      const syncSectionToggle = syncPage.$$('#sync-section-toggle');
      const syncSectionExpandIcon =
          syncSectionToggle.querySelector('cr-expand-button');
      const personalizeSectionToggle =
          syncPage.$$('#personalize-section-toggle');
      const personalizeSectionExpandIcon =
          personalizeSectionToggle.querySelector('cr-expand-button');
      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');

      assertTrue(unifiedConsentToggle.checked == unifiedConsentGiven);
      assertTrue(syncSectionExpandIcon.expanded == syncSectionExpanded);
      assertTrue(syncSectionExpandIcon.disabled == syncSectionDisabled);
      assertTrue(
          personalizeSectionExpandIcon.expanded == personalizeSectionExpanded);
      assertFalse(personalizeSectionExpandIcon.disabled);
    }

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      syncPage = document.createElement('settings-sync-page');
      settings.navigateTo(settings.routes.SYNC);

      document.body.appendChild(syncPage);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(syncPage.$$('#' + settings.PageStatus.CONFIGURE).hidden);
      assertTrue(syncPage.$$('#' + settings.PageStatus.TIMEOUT).hidden);
      assertTrue(syncPage.$$('#' + settings.PageStatus.SPINNER).hidden);

      // Start with Sync All with no encryption selected.
      cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
      Polymer.dom.flush();

      return test_util.waitForRender().then(() => {
        encryptWithGoogle =
            syncPage.$$('cr-radio-button[name="encrypt-with-google"]');
        encryptWithPassphrase =
            syncPage.$$('cr-radio-button[name="encrypt-with-passphrase"]');
        assertTrue(!!encryptWithGoogle);
        assertTrue(!!encryptWithPassphrase);
      });
    });

    teardown(function() {
      syncPage.remove();
    });

    test('NotifiesHandlerOfNavigation', function() {
      function testNavigateAway() {
        settings.navigateTo(settings.routes.PEOPLE);
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
      }

      function testNavigateBack() {
        browserProxy.resetResolver('didNavigateToSyncPage');
        settings.navigateTo(settings.routes.SYNC);
        return browserProxy.whenCalled('didNavigateToSyncPage');
      }

      function testDetach() {
        browserProxy.resetResolver('didNavigateAwayFromSyncPage');
        syncPage.remove();
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
      }

      function testRecreate() {
        browserProxy.resetResolver('didNavigateToSyncPage');
        syncPage = document.createElement('settings-sync-page');
        settings.navigateTo(settings.routes.SYNC);

        document.body.appendChild(syncPage);
        return browserProxy.whenCalled('didNavigateToSyncPage');
      }

      return browserProxy.whenCalled('didNavigateToSyncPage')
          .then(testNavigateAway)
          .then(testNavigateBack)
          .then(testDetach)
          .then(testRecreate);
    });

    test('SyncSectionLayout_NoUnifiedConsent_SignedIn', function() {
      const ironCollapse = syncPage.$$('#sync-section');
      const otherItems = syncPage.$$('#other-sync-items');
      const syncSectionToggle = syncPage.$$('#sync-section-toggle');
      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');

      // When unified-consent is disabled and signed in, sync-section should be
      // visible and open by default. Accordion toggle row should not be present
      // and bottom items should not have classes used for indentation.
      syncPage.syncStatus = {signedIn: true, disabled: false};
      syncPage.unifiedConsentEnabled = false;
      Polymer.dom.flush();
      assertTrue(ironCollapse.opened);
      assertFalse(ironCollapse.hidden);
      assertTrue(syncSectionToggle.hidden);
      assertFalse(otherItems.classList.contains('list-frame'));
      assertFalse(!!otherItems.querySelector('list-item'));

      // The unified consent toggle should be hidden.
      assertTrue(unifiedConsentToggle.hidden);
    });

    test('SyncSectionLayout_UnifiedConsentEnabled_SignedIn', function() {
      const ironCollapse = syncPage.$$('#sync-section');
      const otherItems = syncPage.$$('#other-sync-items');
      const syncSectionToggle = syncPage.$$('#sync-section-toggle');
      const expandIcon = syncSectionToggle.querySelector('cr-expand-button');
      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');

      // When unified-consent is enabled and signed in, sync-section should be
      // visible and open by default. Accordion toggle row should be present,
      // and bottom items should have classes used for indentation.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertTrue(ironCollapse.opened);
      assertFalse(ironCollapse.hidden);
      assertFalse(syncSectionToggle.hidden);
      assertTrue(syncSectionToggle.hasAttribute('actionable'));
      assertTrue(expandIcon.expanded);
      assertFalse(expandIcon.disabled);
      assertTrue(otherItems.classList.contains('list-frame'));
      assertEquals(
          otherItems.querySelectorAll(':scope > .list-item').length, 3);

      // Tapping on the toggle row should toggle ironCollapse.
      syncSectionToggle.click();
      Polymer.dom.flush();
      assertFalse(ironCollapse.opened);
      assertFalse(expandIcon.expanded);

      // Random changes to syncStatus should not expand the section.
      // Regression test for https://crbug.com/869938
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
        statusText: 'UninterestingChange',  // Dummy change to trigger observer.
      };
      assertFalse(ironCollapse.opened);
      assertFalse(expandIcon.expanded);

      syncSectionToggle.click();
      Polymer.dom.flush();
      assertTrue(ironCollapse.opened);
      assertTrue(expandIcon.expanded);

      // The unified consent toggle should be visible.
      assertFalse(unifiedConsentToggle.hidden);

      // Test sync paused state.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: true,
        statusAction: settings.StatusAction.REAUTHENTICATE
      };
      assertTrue(ironCollapse.hidden);

      // Test passphrase error state.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: true,
        statusAction: settings.StatusAction.ENTER_PASSPHRASE
      };
      assertFalse(ironCollapse.hidden);
    });

    test(
        'UnifiedConsentToggleNotifiesHandler_UnifiedConsentEnabled',
        function() {
          const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');
          syncPage.syncStatus = {
            signedIn: true,
            disabled: false,
            hasError: false,
            statusAction: settings.StatusAction.NO_ACTION,
          };
          syncPage.unifiedConsentEnabled = true;
          Polymer.dom.flush();

          assertFalse(unifiedConsentToggle.hidden);
          assertFalse(unifiedConsentToggle.checked);

          unifiedConsentToggle.click();

          return browserProxy.whenCalled('unifiedConsentToggleChanged')
              .then(toggleChecked => {
                assertTrue(toggleChecked);
              });
        });

    test('SyncSectionLayout_UnifiedConsentEnabled_SignoutCollapse', function() {
      const ironCollapse = syncPage.$$('#sync-section');
      const syncSectionToggle = syncPage.$$('#sync-section-toggle');
      const expandIcon = syncSectionToggle.querySelector('cr-expand-button');
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();

      // Sync section is initially open when signed in.
      assertTrue(ironCollapse.opened);
      assertTrue(expandIcon.expanded);

      // Signout collapses the section.
      syncPage.syncStatus = {
        signedIn: false,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      assertFalse(ironCollapse.opened);
      assertFalse(expandIcon.expanded);

      // Signin expands the section.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      assertTrue(ironCollapse.opened);
      assertTrue(expandIcon.expanded);
    });

    test('SyncSectionLayout_UnifiedConsentEnabled_SignedOut', function() {
      const ironCollapse = syncPage.$$('#sync-section');
      const syncSectionToggle = syncPage.$$('#sync-section-toggle');
      const expandIcon = syncSectionToggle.querySelector('cr-expand-button');
      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');

      // When unified-consent is enabled and signed out, sync-section should be
      // hidden, and the accordion toggle row should be visible not actionable.
      syncPage.syncStatus = {
        signedIn: false,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertTrue(ironCollapse.hidden);
      assertFalse(syncSectionToggle.hidden);
      assertFalse(syncSectionToggle.hasAttribute('actionable'));
      assertFalse(expandIcon.expanded);
      assertTrue(expandIcon.disabled);

      // The unified consent toggle should be hidden.
      assertTrue(unifiedConsentToggle.hidden);
    });

    test('SyncSectionLayout_UnifiedConsentEnabled_SyncDisabled', function() {
      const ironCollapse = syncPage.$$('#sync-section');
      const syncSectionToggle = syncPage.$$('#sync-section-toggle');
      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');

      // When unified-consent is enabled and sync is disabled, the sync-section
      // should be hidden.
      syncPage.syncStatus = {
        signedIn: false,
        disabled: true,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertTrue(ironCollapse.hidden);
      assertTrue(syncSectionToggle.hidden);

      // The unified consent toggle should be hidden.
      assertTrue(unifiedConsentToggle.hidden);
    });

    test('InitialLayout_UnifiedConsentGiven_SignedIn', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/true,
          /*signedIn=*/true,
          /*hasError=*/false,
          /*setupInProgress=*/false,
          /*syncSectionExpanded=*/false,
          /*syncSectionDisabled=*/false,
          /*personalizeSectionExpanded=*/false);
    });

    test('InitialLayout_UnifiedConsentGiven_SignedOut', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/true,
          /*signedIn=*/false,
          /*hasError=*/false,
          /*setupInProgress=*/false,
          /*syncSectionExpanded=*/false,
          /*syncSectionDisabled=*/true,
          /*personalizeSectionExpanded=*/false);
    });

    test('InitialLayout_UnifiedConsentGiven_SyncPaused', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/true,
          /*signedIn=*/true,
          /*hasError=*/true,
          /*setupInProgress=*/false,
          /*syncSectionExpanded=*/false,
          /*syncSectionDisabled=*/true,
          /*personalizeSectionExpanded=*/false);
    });

    test('InitialLayout_NoUnifiedConsentGiven_SignedIn', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/false,
          /*signedIn=*/true,
          /*hasError=*/false,
          /*setupInProgress=*/false,
          /*syncSectionExpanded=*/true,
          /*syncSectionDisabled=*/false,
          /*personalizeSectionExpanded=*/true);
    });

    test('InitialLayout_NoUnifiedConsentGiven_SignedOut', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/false,
          /*signedIn=*/false,
          /*hasError=*/false,
          /*setupInProgress=*/false,
          /*syncSectionExpanded=*/false,
          /*syncSectionDisabled=*/true,
          /*personalizeSectionExpanded=*/true);
    });

    test('InitialLayout_NoUnifiedConsentGiven_SyncPaused', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/false,
          /*signedIn=*/true,
          /*hasError=*/true,
          /*setupInProgress=*/false,
          /*syncSectionExpanded=*/false,
          /*syncSectionDisabled=*/true,
          /*personalizeSectionExpanded=*/true);
    });

    test('InitialLayout_SetupInProgress', function() {
      testInitialLayout(
          /*unifiedConsentGiven=*/true,
          /*signedIn=*/true,
          /*hasError=*/false,
          /*setupInProgress=*/true,
          /*syncSectionExpanded=*/true,
          /*syncSectionDisabled=*/false,
          /*personalizeSectionExpanded=*/true);
    });

    test('LoadingAndTimeout', function() {
      const configurePage = syncPage.$$('#' + settings.PageStatus.CONFIGURE);
      const spinnerPage = syncPage.$$('#' + settings.PageStatus.SPINNER);
      const timeoutPage = syncPage.$$('#' + settings.PageStatus.TIMEOUT);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.SPINNER);
      assertTrue(configurePage.hidden);
      assertTrue(timeoutPage.hidden);
      assertFalse(spinnerPage.hidden);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.TIMEOUT);
      assertTrue(configurePage.hidden);
      assertFalse(timeoutPage.hidden);
      assertTrue(spinnerPage.hidden);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(configurePage.hidden);
      assertTrue(timeoutPage.hidden);
      assertTrue(spinnerPage.hidden);

      // Should remain on the CONFIGURE page even if the passphrase failed.
      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.PASSPHRASE_FAILED);
      assertFalse(configurePage.hidden);
      assertTrue(timeoutPage.hidden);
      assertTrue(spinnerPage.hidden);
    });

    test('SettingIndividualDatatypes', function() {
      const syncAllDataTypesControl = syncPage.$.syncAllDataTypesControl;
      assertFalse(syncAllDataTypesControl.disabled);
      assertTrue(syncAllDataTypesControl.checked);

      // Assert that all the individual datatype controls are disabled.
      const datatypeControls =
          syncPage.$$('#configure').querySelectorAll('.list-item cr-toggle');
      for (const control of datatypeControls) {
        assertTrue(control.disabled);
        assertTrue(control.checked);
      }

      // Uncheck the Sync All control.
      syncAllDataTypesControl.click();

      function verifyPrefs(prefs) {
        const expected = getSyncAllPrefs();
        expected.syncAllDataTypes = false;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        cr.webUIListenerCallback('sync-prefs-changed', expected);

        // Assert that all the individual datatype controls are enabled.
        for (const control of datatypeControls) {
          assertFalse(control.disabled);
          assertTrue(control.checked);
        }

        browserProxy.resetResolver('setSyncDatatypes');

        // Test an arbitrarily-selected control (extensions synced control).
        datatypeControls[3].click();
        return browserProxy.whenCalled('setSyncDatatypes')
            .then(function(prefs) {
              const expected = getSyncAllPrefs();
              expected.syncAllDataTypes = false;
              expected.extensionsSynced = false;
              assertEquals(JSON.stringify(expected), JSON.stringify(prefs));
            });
      }
      return browserProxy.whenCalled('setSyncDatatypes').then(verifyPrefs);
    });

    test('RadioBoxesEnabledWhenUnencrypted', function() {
      // Verify that the encryption radio boxes are enabled.
      assertFalse(encryptWithGoogle.disabled);
      assertFalse(encryptWithPassphrase.disabled);

      assertTrue(encryptWithGoogle.checked);

      // Select 'Encrypt with passphrase' to create a new passphrase.
      assertFalse(!!syncPage.$$('#create-password-box'));

      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      // Test that a sync prefs update does not reset the selection.
      cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
      Polymer.dom.flush();
      assertTrue(encryptWithPassphrase.checked);
    });

    test('ClickingLinkDoesNotChangeRadioValue', function() {
      assertFalse(encryptWithPassphrase.disabled);
      assertFalse(encryptWithPassphrase.checked);

      const link = encryptWithPassphrase.querySelector('a[href]');
      assertTrue(!!link);

      // Suppress opening a new tab, since then the test will continue running
      // on a background tab (which has throttled timers) and will timeout.
      link.target = '';
      link.href = '#';
      // Prevent the link from triggering a page navigation when tapped.
      // Breaks the test in Vulcanized mode.
      link.addEventListener('click', function(e) {
        e.preventDefault();
      });

      link.click();

      assertFalse(encryptWithPassphrase.checked);
    });

    test('SaveButtonDisabledWhenPassphraseOrConfirmationEmpty', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      const passphraseInput = syncPage.$$('#passphraseInput');
      const passphraseConfirmationInput =
          syncPage.$$('#passphraseConfirmationInput');

      passphraseInput.value = '';
      passphraseConfirmationInput.value = '';
      assertTrue(saveNewPassphrase.disabled);

      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = '';
      assertTrue(saveNewPassphrase.disabled);

      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'bar';
      assertFalse(saveNewPassphrase.disabled);
    });

    test('CreatingPassphraseMismatchedPassphrase', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      const passphraseInput = syncPage.$$('#passphraseInput');
      const passphraseConfirmationInput =
          syncPage.$$('#passphraseConfirmationInput');
      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'bar';

      saveNewPassphrase.click();
      Polymer.dom.flush();

      assertFalse(passphraseInput.invalid);
      assertTrue(passphraseConfirmationInput.invalid);

      assertFalse(syncPage.syncPrefs.encryptAllData);
    });

    test('CreatingPassphraseValidPassphrase', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      const passphraseInput = syncPage.$$('#passphraseInput');
      const passphraseConfirmationInput =
          syncPage.$$('#passphraseConfirmationInput');
      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'foo';
      saveNewPassphrase.click();

      function verifyPrefs(prefs) {
        const expected = getSyncAllPrefs();
        expected.setNewPassphrase = true;
        expected.passphrase = 'foo';
        expected.encryptAllData = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        expected.fullEncryptionBody = 'Encrypted with custom passphrase';
        cr.webUIListenerCallback('sync-prefs-changed', expected);

        Polymer.dom.flush();

        return test_util.waitForRender(syncPage).then(() => {
          // Need to re-retrieve this, as a different show passphrase radio
          // button is shown once |syncPrefs.fullEncryptionBody| is non-empty.
          encryptWithPassphrase =
              syncPage.$$('cr-radio-button[name="encrypt-with-passphrase"]');

          // Assert that the radio boxes are disabled after encryption enabled.
          assertTrue(syncPage.$$('cr-radio-group').disabled);
          assertEquals('-1', encryptWithGoogle.getAttribute('tabindex'));
          assertEquals('-1', encryptWithPassphrase.getAttribute('tabindex'));
        });
      }
      return browserProxy.whenCalled('setSyncEncryption').then(verifyPrefs);
    });

    test('RadioBoxesHiddenWhenEncrypted', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      prefs.fullEncryptionBody = 'Sync already encrypted.';
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      Polymer.dom.flush();

      assertTrue(syncPage.$.encryptionDescription.hidden);
      assertTrue(syncPage.$.encryptionRadioGroupContainer.hidden);
    });

    test('UserEvents_UnifiedConsent_Encrypted', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      openDatatypeConfigurationWithUnifiedConsent(prefs);

      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');
      // The unified consent toggle is disabled when the data types are
      // encrypted.
      assertTrue(unifiedConsentToggle.disabled);

      assertTrue(prefs.userEventsSynced);
      // History.
      historyToggle = syncPage.$$('#historyToggle');
      assertFalse(historyToggle.disabled);
      assertTrue(historyToggle.checked);
      // User events.
      userEventsToggle = syncPage.$$('#userEventsToggle');
      assertTrue(userEventsToggle.disabled);
      assertFalse(userEventsToggle.checked);
      resetSyncMessageBox = syncPage.$$('#reset-sync-message-box-user-events');
      assertFalse(resetSyncMessageBox.hidden);
    });

    test('UserEvents_UnifiedConsent_NotEncrypted', function() {
      const prefs = getSyncAllPrefs();
      openDatatypeConfigurationWithUnifiedConsent(prefs);

      const unifiedConsentToggle = syncPage.$$('#unifiedConsentToggle');
      // The unified consent toggle is enabled when the data types are not
      // encrypted.
      assertFalse(unifiedConsentToggle.disabled);

      assertTrue(prefs.userEventsSynced);
      // Check history toggle.
      historyToggle = syncPage.$$('#historyToggle');
      assertFalse(historyToggle.disabled);
      assertTrue(historyToggle.checked);
      // Check user events toggle.
      userEventsToggle = syncPage.$$('#userEventsToggle');
      assertFalse(userEventsToggle.disabled);
      assertTrue(userEventsToggle.checked);
      resetSyncMessageBox = syncPage.$$('#reset-sync-message-box-user-events');
      assertTrue(resetSyncMessageBox.hidden);

      // Toggling history also toggles user events.
      // Turn history off.
      historyToggle.click();
      cr.webUIListenerCallback('sync-prefs-changed', prefs);
      assertFalse(historyToggle.checked);
      assertTrue(userEventsToggle.disabled);
      assertFalse(userEventsToggle.checked);
      assertTrue(resetSyncMessageBox.hidden);
      assertTrue(prefs.userEventsSynced);
      // Turn history on.
      historyToggle.click();
      cr.webUIListenerCallback('sync-prefs-changed', prefs);
      assertTrue(historyToggle.checked);
      assertFalse(userEventsToggle.disabled);
      assertTrue(userEventsToggle.checked);
      assertTrue(prefs.userEventsSynced);

      // Toggling user events also toggles the sync preference.
      userEventsToggle.click();
      cr.webUIListenerCallback('sync-prefs-changed', prefs);
      assertFalse(userEventsToggle.disabled);
      assertFalse(userEventsToggle.checked);
      assertFalse(prefs.userEventsSynced);
      assertTrue(resetSyncMessageBox.hidden);
    });

    test(
        'ExistingPassphraseSubmitButtonDisabledWhenExistingPassphraseEmpty',
        function() {
          const prefs = getSyncAllPrefs();
          prefs.encryptAllData = true;
          prefs.passphraseRequired = true;
          cr.webUIListenerCallback('sync-prefs-changed', prefs);

          syncPage.unifiedConsentEnabled = false;
          Polymer.dom.flush();

          const existingPassphraseInput =
              syncPage.$$('#existingPassphraseInput');
          const submitExistingPassphrase =
              syncPage.$$('#submitExistingPassphrase');

          existingPassphraseInput.value = '';
          assertTrue(submitExistingPassphrase.disabled);

          existingPassphraseInput.value = 'foo';
          assertFalse(submitExistingPassphrase.disabled);
        });

    test('EnterExistingWrongPassphrase', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      syncPage.unifiedConsentEnabled = false;
      Polymer.dom.flush();

      const existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
      assertTrue(!!existingPassphraseInput);
      existingPassphraseInput.value = 'wrong';
      browserProxy.encryptionResponse = settings.PageStatus.PASSPHRASE_FAILED;

      const submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
      assertTrue(!!submitExistingPassphrase);
      submitExistingPassphrase.click();

      return browserProxy.whenCalled('setSyncEncryption').then(function(prefs) {
        const expected = getSyncAllPrefs();
        expected.setNewPassphrase = false;
        expected.passphrase = 'wrong';
        expected.encryptAllData = true;
        expected.passphraseRequired = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        Polymer.dom.flush();

        assertTrue(existingPassphraseInput.invalid);
      });
    });

    test('EnterExistingCorrectPassphrase', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      syncPage.unifiedConsentEnabled = false;
      Polymer.dom.flush();

      const existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
      assertTrue(!!existingPassphraseInput);
      existingPassphraseInput.value = 'right';
      browserProxy.encryptionResponse = settings.PageStatus.CONFIGURE;

      const submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
      assertTrue(!!submitExistingPassphrase);
      submitExistingPassphrase.click();

      return browserProxy.whenCalled('setSyncEncryption').then(function(prefs) {
        const expected = getSyncAllPrefs();
        expected.setNewPassphrase = false;
        expected.passphrase = 'right';
        expected.encryptAllData = true;
        expected.passphraseRequired = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        const newPrefs = getSyncAllPrefs();
        newPrefs.encryptAllData = true;
        cr.webUIListenerCallback('sync-prefs-changed', newPrefs);

        Polymer.dom.flush();

        // Verify that the encryption radio boxes are shown but disabled.
        assertTrue(syncPage.$$('cr-radio-group').disabled);
        assertEquals('-1', encryptWithGoogle.getAttribute('tabindex'));
        assertEquals('-1', encryptWithPassphrase.getAttribute('tabindex'));
      });
    });

    if (!cr.isChromeOS) {
      test('FirstTimeSetupNotification', function() {
        assertTrue(!!syncPage.$.toast);
        assertFalse(syncPage.$.toast.open);
        syncPage.syncStatus = {setupInProgress: true};
        Polymer.dom.flush();
        assertTrue(syncPage.$.toast.open);

        syncPage.$.toast.querySelector('paper-button').click();

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertTrue(abort);
            });
      });

      test('ShowAccountRow', function() {
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.diceEnabled = true;
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.unifiedConsentEnabled = true;
        syncPage.syncStatus = {signinAllowed: false, syncSystemEnabled: false};
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.syncStatus = {signinAllowed: true, syncSystemEnabled: true};
        Polymer.dom.flush();
        assertTrue(!!syncPage.$$('settings-sync-account-control'));
      });
    }
  });
});
