// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_privacy_page', function() {
  /**
   * @param {!Element} element
   * @param {boolean} displayed
   */
  function assertVisible(element, displayed) {
    assertEquals(
        displayed, window.getComputedStyle(element)['display'] != 'none');
  }

  /** @implements {settings.ClearBrowsingDataBrowserProxy} */
  class TestClearBrowsingDataBrowserProxy extends TestBrowserProxy {
    constructor() {
      super(['initialize', 'clearBrowsingData']);

      /**
       * The promise to return from |clearBrowsingData|.
       * Allows testing code to test what happens after the call is made, and
       * before the browser responds.
       * @private {?Promise}
       */
      this.clearBrowsingDataPromise_ = null;
    }

    /** @param {!Promise} promise */
    setClearBrowsingDataPromise(promise) {
      this.clearBrowsingDataPromise_ = promise;
    }

    /** @override */
    clearBrowsingData(dataTypes, timePeriod) {
      this.methodCalled('clearBrowsingData', [dataTypes, timePeriod]);
      cr.webUIListenerCallback('browsing-data-removing', true);
      return this.clearBrowsingDataPromise_ !== null ?
          this.clearBrowsingDataPromise_ :
          Promise.resolve();
    }

    /** @override */
    initialize() {
      this.methodCalled('initialize');
      return Promise.resolve(false);
    }
  }

  function getClearBrowsingDataPrefs() {
    return {
      browser: {
        clear_data: {
          time_period: {
            key: 'browser.clear_data.time_period',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          time_period_basic: {
            key: 'browser.clear_data.time_period_basic',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          browsing_history: {
            key: 'browser.clear_data.browsing_history',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cookies: {
            key: 'browser.clear_data.cookies',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cookies_basic: {
            key: 'browser.clear_data.cookies_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          cache_basic: {
            key: 'browser.clear_data.cache_basic',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        last_clear_browsing_data_tab: {
          key: 'browser.last_clear_browsing_data_tab',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
      }
    };
  }

  function registerUMALoggingTests() {
    suite('PrivacyPageUMACheck', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('LogMangeCerfificatesClick', function() {
        page.$$('#manageCertificates').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES,
                  result);
            });
      });

      test('LogClearBrowsingClick', function() {
        page.$$('#clearBrowsingData').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_CLEAR_BROWSING_DATA,
                  result);
            });
      });

      test('LogDoNotTrackClick', function() {
        page.$$('#doNotTrack').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_DO_NOT_TRACK,
                  result);
            });
      });

      test('LogCanMakePaymentToggleClick', function() {
        page.$$('#canMakePaymentToggle').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_PAYMENT_METHOD,
                  result);
            });
      });

      test('LogSiteSettingsSubpageClick', function() {
        page.$$('#site-settings-subpage-trigger').click();
        return testBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS,
                  result);
            });
      });
    });
  }

  function registerNativeCertificateManagerTests() {
    suite('NativeCertificateManager', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('NativeCertificateManager', function() {
        page.$$('#manageCertificates').click();
        return testBrowserProxy.whenCalled('showManageSSLCertificates');
      });
    });
  }

  function registerPrivacyPageTests() {
    suite('PrivacyPage', function() {
      /** @type {SettingsPrivacyPageElement} */
      let page;

      setup(function() {
        page = document.createElement('settings-privacy-page');
        page.prefs = {
          signin: {
            allowed_on_next_startup:
                {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
          },
        };
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('showClearBrowsingDataDialog', function() {
        assertFalse(!!page.$$('settings-clear-browsing-data-dialog'));
        page.$$('#clearBrowsingData').click();
        Polymer.dom.flush();

        const dialog = page.$$('settings-clear-browsing-data-dialog');
        assertTrue(!!dialog);

        // Ensure that the dialog is fully opened before returning from this
        // test, otherwise asynchronous code run in attached() can cause flaky
        // errors.
        return test_util.whenAttributeIs(
            dialog.$$('#clearBrowsingDataDialog'), 'open', '');
      });

      if (!cr.isChromeOS) {
        test('signinAllowedToggle', function() {
          const toggle = page.$.signinAllowedToggle;

          page.syncStatus = {signedIn: false};
          // Check initial setup.
          assertTrue(toggle.checked);
          assertTrue(page.prefs.signin.allowed_on_next_startup.value);
          assertFalse(page.$.toast.open);

          // When the user is signed out, clicking the toggle should work
          // normally and the restart toast should be opened.
          toggle.click();
          assertFalse(toggle.checked);
          assertFalse(page.prefs.signin.allowed_on_next_startup.value);
          assertTrue(page.$.toast.open);

          // Clicking it again, turns the toggle back on. The toast remains
          // open.
          toggle.click();
          assertTrue(toggle.checked);
          assertTrue(page.prefs.signin.allowed_on_next_startup.value);
          assertTrue(page.$.toast.open);

          // Reset toast.
          page.showRestart_ = false;
          assertFalse(page.$.toast.open);

          page.syncStatus = {signedIn: true};
          // When the user is signed in, clicking the toggle should open the
          // sign-out dialog.
          assertFalse(!!page.$$('settings-signout-dialog'));
          toggle.click();
          return test_util.eventToPromise('cr-dialog-open', page)
              .then(function() {
                Polymer.dom.flush();
                // The toggle remains on.
                assertTrue(toggle.checked);
                assertTrue(page.prefs.signin.allowed_on_next_startup.value);
                assertFalse(page.$.toast.open);

                const signoutDialog = page.$$('settings-signout-dialog');
                assertTrue(!!signoutDialog);
                assertTrue(signoutDialog.$$('#dialog').open);

                // The user clicks cancel.
                const cancel = signoutDialog.$$('#disconnectCancel');
                cancel.click();

                return test_util.eventToPromise('close', signoutDialog);
              })
              .then(function() {
                Polymer.dom.flush();
                assertFalse(!!page.$$('settings-signout-dialog'));

                // After the dialog is closed, the toggle remains turned on.
                assertTrue(toggle.checked);
                assertTrue(page.prefs.signin.allowed_on_next_startup.value);
                assertFalse(page.$.toast.open);

                // The user clicks the toggle again.
                toggle.click();
                return test_util.eventToPromise('cr-dialog-open', page);
              })
              .then(function() {
                Polymer.dom.flush();
                const signoutDialog = page.$$('settings-signout-dialog');
                assertTrue(!!signoutDialog);
                assertTrue(signoutDialog.$$('#dialog').open);

                // The user clicks confirm, which signs them out.
                const disconnectConfirm =
                    signoutDialog.$$('#disconnectConfirm');
                disconnectConfirm.click();

                return test_util.eventToPromise('close', signoutDialog);
              })
              .then(function() {
                Polymer.dom.flush();
                // After the dialog is closed, the toggle is turned off and the
                // toast is shown.
                assertFalse(toggle.checked);
                assertFalse(page.prefs.signin.allowed_on_next_startup.value);
                assertTrue(page.$.toast.open);
              });
        });
      }
    });
  }

  function registerClearBrowsingDataTestsDice() {
    suite('ClearBrowsingDataDice', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      let testBrowserProxy;

      /** @type {TestSyncBrowserProxy} */
      let testSyncBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      let element;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          diceEnabled: true,
        });
      });

      setup(function() {
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        testSyncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        element.set('prefs', getClearBrowsingDataPrefs());
        document.body.appendChild(element);
        return testBrowserProxy.whenCalled('initialize').then(() => {
          assertTrue(element.$$('#clearBrowsingDataDialog').open);
        });
      });

      teardown(function() {
        element.remove();
      });

      test('ClearBrowsingDataSyncAccountInfoDice', function() {
        // Not syncing: the footer is hidden.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: false,
          hasError: false,
        });
        Polymer.dom.flush();
        const footer = element.$$('#clearBrowsingDataDialog [slot=footer]');
        assertTrue(footer.hidden);

        // Syncing: the footer is shown, with the normal sync info.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: false,
        });
        Polymer.dom.flush();
        assertFalse(footer.hidden);
        assertVisible(element.$$('#sync-info'), true);
        assertVisible(element.$$('#sync-paused-info'), false);
        assertVisible(element.$$('#sync-passphrase-error-info'), false);
        assertVisible(element.$$('#sync-other-error-info'), false);

        // Sync is paused.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.REAUTHENTICATE,
        });
        Polymer.dom.flush();
        assertVisible(element.$$('#sync-info'), false);
        assertVisible(element.$$('#sync-paused-info'), true);
        assertVisible(element.$$('#sync-passphrase-error-info'), false);
        assertVisible(element.$$('#sync-other-error-info'), false);

        // Sync passphrase error.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        });
        Polymer.dom.flush();
        assertVisible(element.$$('#sync-info'), false);
        assertVisible(element.$$('#sync-paused-info'), false);
        assertVisible(element.$$('#sync-passphrase-error-info'), true);
        assertVisible(element.$$('#sync-other-error-info'), false);

        // Other sync error.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.NO_ACTION,
        });
        Polymer.dom.flush();
        assertVisible(element.$$('#sync-info'), false);
        assertVisible(element.$$('#sync-paused-info'), false);
        assertVisible(element.$$('#sync-passphrase-error-info'), false);
        assertVisible(element.$$('#sync-other-error-info'), true);
      });

      test('ClearBrowsingDataPauseSyncDice', function() {
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: false,
        });
        Polymer.dom.flush();
        assertFalse(
            element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);
        const syncInfo = element.$$('#sync-info');
        assertVisible(syncInfo, true);
        const signoutLink = syncInfo.querySelector('a[href]');
        assertTrue(!!signoutLink);
        assertEquals(0, testSyncBrowserProxy.getCallCount('pauseSync'));
        signoutLink.click();
        assertEquals(1, testSyncBrowserProxy.getCallCount('pauseSync'));
      });

      test('ClearBrowsingDataStartSignInDice', function() {
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.REAUTHENTICATE,
        });
        Polymer.dom.flush();
        assertFalse(
            element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);
        const syncInfo = element.$$('#sync-paused-info');
        assertVisible(syncInfo, true);
        const signinLink = syncInfo.querySelector('a[href]');
        assertTrue(!!signinLink);
        assertEquals(0, testSyncBrowserProxy.getCallCount('startSignIn'));
        signinLink.click();
        assertEquals(1, testSyncBrowserProxy.getCallCount('startSignIn'));
      });

      test('ClearBrowsingDataHandlePassphraseErrorDice', function() {
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        });
        Polymer.dom.flush();
        assertFalse(
            element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);
        const syncInfo = element.$$('#sync-passphrase-error-info');
        assertVisible(syncInfo, true);
        const passphraseLink = syncInfo.querySelector('a[href]');
        assertTrue(!!passphraseLink);
        passphraseLink.click();
        assertEquals(settings.routes.SYNC, settings.getCurrentRoute());
      });
    });
  }

  function registerClearBrowsingDataTests() {
    suite('ClearBrowsingData', function() {
      /** @type {settings.TestClearBrowsingDataBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsClearBrowsingDataDialogElement} */
      let element;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          diceEnabled: false,
        });
      });

      setup(function() {
        testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
        settings.ClearBrowsingDataBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        element = document.createElement('settings-clear-browsing-data-dialog');
        element.set('prefs', getClearBrowsingDataPrefs());
        document.body.appendChild(element);
        return testBrowserProxy.whenCalled('initialize');
      });

      teardown(function() {
        element.remove();
      });

      test('ClearBrowsingDataTap', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const cancelButton = element.$$('.cancel-button');
        assertTrue(!!cancelButton);
        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        const spinner = element.$$('paper-spinner-lite');
        assertTrue(!!spinner);

        // Select a datatype for deletion to enable the clear button.
        const cookieCheckbox = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckbox);
        cookieCheckbox.$.checkbox.click();

        assertFalse(cancelButton.disabled);
        assertFalse(actionButton.disabled);
        assertFalse(spinner.active);

        const promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        actionButton.click();

        return testBrowserProxy.whenCalled('clearBrowsingData')
            .then(function(args) {
              const dataTypes = args[0];
              const timePeriod = args[1];
              assertEquals(1, dataTypes.length);
              assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
              assertTrue(element.$$('#clearBrowsingDataDialog').open);
              assertTrue(cancelButton.disabled);
              assertTrue(actionButton.disabled);
              assertTrue(spinner.active);

              // Simulate signal from browser indicating that clearing has
              // completed.
              cr.webUIListenerCallback('browsing-data-removing', false);
              promiseResolver.resolve();
              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            })
            .then(function() {
              assertFalse(element.$$('#clearBrowsingDataDialog').open);
              assertFalse(cancelButton.disabled);
              assertFalse(actionButton.disabled);
              assertFalse(spinner.active);
              assertFalse(!!element.$$('#notice'));
            });
      });

      test('ClearBrowsingDataClearButton', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);
        const cookieCheckboxBasic = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckboxBasic);
        // Initially the button is disabled because all checkboxes are off.
        assertTrue(actionButton.disabled);
        // The button gets enabled if any checkbox is selected.
        cookieCheckboxBasic.$.checkbox.click();
        assertTrue(cookieCheckboxBasic.checked);
        assertFalse(actionButton.disabled);
        // Switching to advanced disables the button.
        element.$$('cr-tabs').selected = 1;
        assertTrue(actionButton.disabled);
        // Switching back enables it again.
        element.$$('cr-tabs').selected = 0;
        assertFalse(actionButton.disabled);
      });

      test('showHistoryDeletionDialog', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);
        const actionButton = element.$$('.action-button');
        assertTrue(!!actionButton);

        // Select a datatype for deletion to enable the clear button.
        const cookieCheckbox = element.$$('#cookiesCheckboxBasic');
        assertTrue(!!cookieCheckbox);
        cookieCheckbox.$.checkbox.click();
        assertFalse(actionButton.disabled);

        const promiseResolver = new PromiseResolver();
        testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
        actionButton.click();

        return testBrowserProxy.whenCalled('clearBrowsingData')
            .then(function() {
              // Passing showNotice = true should trigger the notice about other
              // forms of browsing history to open, and the dialog to stay open.
              promiseResolver.resolve(true /* showNotice */);

              // Yields to the message loop to allow the callback chain of the
              // Promise that was just resolved to execute before the
              // assertions.
            })
            .then(function() {
              Polymer.dom.flush();
              const notice = element.$$('#notice');
              assertTrue(!!notice);
              const noticeActionButton = notice.$$('.action-button');
              assertTrue(!!noticeActionButton);

              assertTrue(element.$$('#clearBrowsingDataDialog').open);
              assertTrue(notice.$$('#dialog').open);

              noticeActionButton.click();

              return new Promise(function(resolve, reject) {
                // Tapping the action button will close the notice. Move to the
                // end of the message loop to allow the closing event to
                // propagate to the parent dialog. The parent dialog should
                // subsequently close as well.
                setTimeout(function() {
                  const notice = element.$$('#notice');
                  assertFalse(!!notice);
                  assertFalse(element.$$('#clearBrowsingDataDialog').open);
                  resolve();
                }, 0);
              });
            });
      });

      test('Counters', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        const checkbox = element.$$('#cacheCheckboxBasic');
        assertEquals('browser.clear_data.cache_basic', checkbox.pref.key);

        // Simulate a browsing data counter result for history. This checkbox's
        // sublabel should be updated.
        cr.webUIListenerCallback(
            'update-counter-text', checkbox.pref.key, 'result');
        assertEquals('result', checkbox.subLabel);
      });

      test('history rows are hidden for supervised users', function() {
        assertFalse(loadTimeData.getBoolean('isSupervised'));
        assertFalse(element.$$('#browsingCheckbox').hidden);
        assertFalse(element.$$('#browsingCheckboxBasic').hidden);
        assertFalse(element.$$('#downloadCheckbox').hidden);

        element.remove();
        testBrowserProxy.reset();
        loadTimeData.overrideValues({isSupervised: true});

        element = document.createElement('settings-clear-browsing-data-dialog');
        document.body.appendChild(element);
        Polymer.dom.flush();

        return testBrowserProxy.whenCalled('initialize').then(function() {
          assertTrue(element.$$('#browsingCheckbox').hidden);
          assertTrue(element.$$('#browsingCheckboxBasic').hidden);
          assertTrue(element.$$('#downloadCheckbox').hidden);
        });
      });

      // When Dice is disabled, the footer is never shown.
      test('ClearBrowsingDataSyncAccountInfo', function() {
        assertTrue(element.$$('#clearBrowsingDataDialog').open);

        // Not syncing.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: false,
          hasError: false,
        });
        Polymer.dom.flush();
        assertTrue(element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);

        // Syncing.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: false,
        });
        Polymer.dom.flush();
        assertTrue(element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);

        // Sync passphrase error.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.ENTER_PASSPHRASE,
        });
        Polymer.dom.flush();
        assertTrue(element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);

        // Other sync error.
        cr.webUIListenerCallback('sync-status-changed', {
          signedIn: true,
          hasError: true,
          statusAction: settings.StatusAction.NO_ACTION,
        });
        Polymer.dom.flush();
        assertTrue(element.$$('#clearBrowsingDataDialog [slot=footer]').hidden);
      });
    });
  }

  function registerPrivacyPageSoundTests() {
    suite('PrivacyPageSound', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      function flushAsync() {
        Polymer.dom.flush();
        return new Promise(resolve => {
          page.async(resolve);
        });
      }

      function getToggleElement() {
        return page.$$('settings-animated-pages')
            .queryEffectiveChildren('settings-subpage')
            .queryEffectiveChildren('#block-autoplay-setting');
      }

      setup(() => {
        loadTimeData.overrideValues({
          enableBlockAutoplayContentSetting: true
        });

        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();

        settings.router.navigateTo(settings.routes.SITE_SETTINGS_SOUND);
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
        return flushAsync();
      });

      teardown(() => {
        page.remove();
      });

      test('UpdateStatus', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Toggle the pref off.
          cr.webUIListenerCallback(
              'onBlockAutoplayStatusChanged',
              {pref: {value: false}, enabled: true});

          return flushAsync().then(() => {
            // Check that we are off and enabled.
            assertFalse(getToggleElement().hasAttribute('disabled'));
            assertFalse(getToggleElement().hasAttribute('checked'));

            // Disable the autoplay status toggle.
            cr.webUIListenerCallback(
                'onBlockAutoplayStatusChanged',
                {pref: {value: false}, enabled: false});

            return flushAsync().then(() => {
              // Check that we are off and disabled.
              assertTrue(getToggleElement().hasAttribute('disabled'));
              assertFalse(getToggleElement().hasAttribute('checked'));
            });
          });
        });
      });

      test('Hidden', () => {
        assertTrue(
            loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
        assertFalse(getToggleElement().hidden);

        loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});

        page.remove();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);

        return flushAsync().then(() => {
          assertFalse(
              loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
          assertTrue(getToggleElement().hidden);
        });
      });

      test('Click', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Click on the toggle and wait for the proxy to be called.
          getToggleElement().click();
          return testBrowserProxy.whenCalled('setBlockAutoplayEnabled')
              .then((enabled) => {
                assertFalse(enabled);
              });
        });
      });
    });
  }

  if (cr.isMac || cr.isWindows) {
    registerNativeCertificateManagerTests();
  }

  if (!cr.isChromeOS) {
    registerClearBrowsingDataTestsDice();
  }

  registerClearBrowsingDataTests();
  registerPrivacyPageTests();
  registerPrivacyPageSoundTests();
  registerUMALoggingTests();
});
