// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sync_account_control', function() {
  /**
   * @param {!Element} element
   * @param {boolean} displayed
   */
  function assertVisible(element, displayed) {
    assertEquals(
        displayed, window.getComputedStyle(element)['display'] != 'none');
  }

  suite('SyncAccountControl', function() {
    let peoplePage = null;
    let browserProxy = null;
    let testElement = null;

    /**
     * @param {number} count
     * @param {boolean} signedIn
     */
    function forcePromoResetWithCount(count, signedIn) {
      browserProxy.setImpressionCount(count);
      // Flipping syncStatus.signedIn will force promo state to be reset.
      testElement.syncStatus = {signedIn: !signedIn};
      testElement.syncStatus = {signedIn: signedIn};
    }

    suiteSetup(function() {
      // Force UIs to think DICE is enabled for this profile.
      loadTimeData.overrideValues({
        diceEnabled: true,
      });
    });

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      testElement = document.createElement('settings-sync-account-control');
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'fakeUsername'
      };
      document.body.appendChild(testElement);

      return browserProxy.whenCalled('getStoredAccounts').then(() => {
        Polymer.dom.flush();
      });
    });

    teardown(function() {
      testElement.remove();
    });

    test('promo shows/hides in the right states', function() {
      // Not signed in, no accounts, will show banner.
      sync_test_util.simulateStoredAccounts([]);
      forcePromoResetWithCount(0, false);
      const banner = testElement.$$('#banner');
      assertVisible(banner, true);
      // Flipping signedIn in forcePromoResetWithCount should increment count.
      return browserProxy.whenCalled('incrementPromoImpressionCount')
          .then(() => {
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
            assertVisible(banner, false);

            // Not signed in, has accounts, will show banner.
            sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
            forcePromoResetWithCount(0, false);
            assertVisible(banner, true);
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
            assertVisible(banner, false);

            // signed in, banners never show.
            sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
            forcePromoResetWithCount(0, true);
            assertVisible(banner, false);
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, true);
            assertVisible(banner, false);
          });
    });

    test('not signed in and no stored accounts', function() {
      testElement.syncStatus = {signedIn: false, signedInUsername: ''};
      sync_test_util.simulateStoredAccounts([]);

      assertVisible(testElement.$$('#promo-header'), true);
      assertVisible(testElement.$$('#avatar-row'), false);
      // TODO (rbpotter): Remove this conditional when the migration to
      // Polymer 2 is completed.
      if (Polymer.DomIf)
        assertEquals(null, testElement.$$('#menu'));
      else
        assertVisible(testElement.$$('#menu'), false);
      assertVisible(testElement.$$('#sign-in'), true);

      testElement.$$('#sign-in').click();
      return browserProxy.whenCalled('startSignIn');
    });

    test('not signed in but has stored accounts', function() {
      testElement.syncStatus = {
        signedIn: false,
        signedInUsername: '',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        disabled: false,
      };
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'fooName',
          givenName: 'foo',
          email: 'foo@foo.com',
        },
        {
          fullName: 'barName',
          givenName: 'bar',
          email: 'bar@bar.com',
        },
      ]);

      const userInfo = testElement.$$('#user-info');
      const syncButton = testElement.$$('#sync-button');

      // Avatar row shows the right account.
      assertVisible(testElement.$$('#promo-header'), true);
      assertVisible(testElement.$$('#avatar-row'), true);
      assertTrue(userInfo.textContent.includes('fooName'));
      assertTrue(userInfo.textContent.includes('foo@foo.com'));
      assertFalse(userInfo.textContent.includes('barName'));
      assertFalse(userInfo.textContent.includes('bar@bar.com'));

      // Menu contains the right items.
      assertTrue(!!testElement.$$('#menu'));
      assertFalse(testElement.$$('#menu').open);
      const items = testElement.root.querySelectorAll('.dropdown-item');
      assertEquals(4, items.length);
      assertTrue(items[0].textContent.includes('foo@foo.com'));
      assertTrue(items[1].textContent.includes('bar@bar.com'));
      assertEquals(items[2].id, 'sign-in-item');
      assertEquals(items[3].id, 'sign-out-item');

      // "sync to" button is showing the correct name and syncs with the
      // correct account when clicked.
      assertVisible(syncButton, true);
      assertVisible(testElement.$$('#turn-off'), false);
      syncButton.click();
      Polymer.dom.flush();

      return browserProxy.whenCalled('startSyncingWithEmail')
          .then((args) => {
            const email = args[0];
            const isDefaultPromoAccount = args[1];

            assertEquals(email, 'foo@foo.com');
            assertEquals(isDefaultPromoAccount, true);

            assertVisible(testElement.$$('paper-icon-button-light'), true);
            assertTrue(testElement.$$('#sync-icon-container').hidden);

            testElement.$$('#dropdown-arrow').click();
            Polymer.dom.flush();
            assertTrue(testElement.$$('#menu').open);

            // Switching selected account will update UI with the right name and
            // email.
            items[1].click();
            Polymer.dom.flush();
            assertFalse(userInfo.textContent.includes('fooName'));
            assertFalse(userInfo.textContent.includes('foo@foo.com'));
            assertTrue(userInfo.textContent.includes('barName'));
            assertTrue(userInfo.textContent.includes('bar@bar.com'));
            assertVisible(syncButton, true);

            browserProxy.resetResolver('startSyncingWithEmail');
            syncButton.click();
            Polymer.dom.flush();

            return browserProxy.whenCalled('startSyncingWithEmail');
          })
          .then((args) => {
            const email = args[0];
            const isDefaultPromoAccount = args[1];
            assertEquals(email, 'bar@bar.com');
            assertEquals(isDefaultPromoAccount, false);

            // Tapping the last menu item will initiate sign-in.
            items[2].click();
            return browserProxy.whenCalled('startSignIn');
          });
    });

    test('signed in', function() {
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'fooName',
          givenName: 'foo',
          email: 'foo@foo.com',
        },
        {
          fullName: 'barName',
          givenName: 'bar',
          email: 'bar@bar.com',
        },
      ]);

      assertVisible(testElement.$$('#avatar-row'), true);
      assertVisible(testElement.$$('paper-icon-button-light'), false);
      assertVisible(testElement.$$('#promo-header'), false);
      assertFalse(testElement.$$('#sync-icon-container').hidden);

      assertFalse(!!testElement.$$('#menu'));

      const userInfo = testElement.$$('#user-info');
      assertTrue(userInfo.textContent.includes('barName'));
      assertTrue(userInfo.textContent.includes('bar@bar.com'));
      assertFalse(userInfo.textContent.includes('fooName'));
      assertFalse(userInfo.textContent.includes('foo@foo.com'));

      assertVisible(testElement.$$('#sync-button'), false);
      assertVisible(testElement.$$('#turn-off'), true);
      assertVisible(testElement.$$('#sync-paused-button'), false);

      testElement.$$('#avatar-row .secondary-button').click();
      Polymer.dom.flush();

      assertEquals(settings.getCurrentRoute(), settings.routes.SIGN_OUT);

      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.CONFIRM_SYNC_SETTINGS,
        disabled: false,
      };
      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-problem'));
      assertTrue(!!testElement.$$('[icon=\'settings:sync-problem\']'));
      let displayedText =
          userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync isn\'t working'));

      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-paused'));
      assertTrue(!!testElement.$$('[icon=\'settings:sync-disabled\']'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync is paused'));
      // Not embedded in a subpage, so there is no sync-paused button.
      assertVisible(testElement.$$('#sync-paused-button'), false);

      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: true,
      };

      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-disabled'));
      assertTrue(!!testElement.$$('[icon=\'cr:sync\']'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync disabled'));

      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.REAUTHENTICATE,
        hasError: false,
        hasUnrecoverableError: true,
        disabled: false,
      };
      assertTrue(testElement.$$('#sync-icon-container')
                     .classList.contains('sync-problem'));
      assertTrue(!!testElement.$$('[icon=\'settings:sync-problem\']'));
      displayedText = userInfo.querySelector('span:not([hidden])').textContent;
      assertFalse(displayedText.includes('barName'));
      assertFalse(displayedText.includes('fooName'));
      assertTrue(displayedText.includes('Sync isn\'t working'));
    });

    test('embedded in another page', function() {
      testElement.embeddedInSubpage = true;
      forcePromoResetWithCount(100, false);
      const banner = testElement.$$('#banner');
      assertVisible(banner, true);

      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };

      assertVisible(testElement.$$('#turn-off'), false);
      assertVisible(testElement.$$('#sync-paused-button'), false);

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertVisible(testElement.$$('#turn-off'), false);
      assertVisible(testElement.$$('#sync-paused-button'), true);

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: true,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertVisible(testElement.$$('#turn-off'), false);
      assertVisible(testElement.$$('#sync-paused-button'), true);

      testElement.embeddedInSubpage = true;
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: true,
        statusAction: settings.StatusAction.NO_ACTION,
        disabled: false,
      };
      assertVisible(testElement.$$('#turn-off'), false);
      assertVisible(testElement.$$('#sync-paused-button'), false);
    });

    test('hide buttons', function() {
      testElement.hideButtons = true;
      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: settings.StatusAction.NO_ACTION,
        hasError: false,
        hasUnrecoverableError: false,
        disabled: false,
      };

      assertVisible(testElement.$$('#turn-off'), false);
      assertVisible(testElement.$$('#sync-paused-button'), false);

      testElement.syncStatus = {
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        hasError: true,
        hasUnrecoverableError: false,
        statusAction: settings.StatusAction.REAUTHENTICATE,
        disabled: false,
      };
      assertVisible(testElement.$$('#turn-off'), false);
      assertVisible(testElement.$$('#sync-paused-button'), false);
    });
  });
});
