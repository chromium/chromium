// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrActionMenuElement, SettingsSyncAccountControlElement, StoredAccount} from 'chrome://settings/settings.js';
import {loadTimeData, resetRouterForTesting, Router, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {simulateStoredAccounts} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';


suite('SyncAccountControl', function() {
  let browserProxy: TestSyncBrowserProxy;
  let testElement: SettingsSyncAccountControlElement;

  setup(async function() {
    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: false});
    resetRouterForTesting();

    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-sync-account-control');
    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'foo@foo.com',
      statusAction: StatusAction.NO_ACTION,
    };
    testElement.prefs = {
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
    };

    document.body.appendChild(testElement);

    await browserProxy.whenCalled('getStoredAccounts');
    flush();
    simulateStoredAccounts([
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
  });

  teardown(function() {
    testElement.remove();
  });

  test('promo header is visible', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
      signedInUsername: '',
      statusAction: StatusAction.NO_ACTION,
    };
    testElement.promoLabelWithNoAccount = testElement.promoLabelWithAccount =
        'title';
    simulateStoredAccounts([]);
    assertTrue(isChildVisible(testElement, '#promo-header'));
  });

  test('not signed in and no stored accounts', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
      signedInUsername: '',
      statusAction: StatusAction.NO_ACTION,
    };
    simulateStoredAccounts([]);

    assertTrue(isChildVisible(testElement, '#promo-header'));
    assertFalse(isChildVisible(testElement, '#avatar-row'));

    // Chrome OS does not use the account switch menu.
    assertFalse(isChildVisible(testElement, '#menu'));

    assertTrue(isChildVisible(testElement, '#signIn'));

    testElement.$.signIn.click();

    return browserProxy.whenCalled('startSignIn');
  });

  test('not signed in but has stored accounts', async function() {
    loadTimeData.overrideValues({isSecondaryUser: true});
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SIGNED_OUT,
      signedInUsername: '',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
      disabled: false,
    };
    simulateStoredAccounts([
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

    const userInfo =
        testElement.shadowRoot!.querySelector<HTMLElement>('#user-info')!;
    const syncButton =
        testElement.shadowRoot!.querySelector<HTMLElement>('#sync-button')!;

    // Avatar row shows the right account.
    assertTrue(isChildVisible(testElement, '#promo-header'));
    assertTrue(isChildVisible(testElement, '#avatar-row'));
    assertTrue(userInfo.textContent.includes('fooName'));
    assertTrue(userInfo.textContent.includes('foo@foo.com'));
    assertFalse(userInfo.textContent.includes('barName'));
    assertFalse(userInfo.textContent.includes('bar@bar.com'));

    // Menu contains the right items.
    assertTrue(!!testElement.shadowRoot!.querySelector('#menu'));
    assertFalse(
        testElement.shadowRoot!.querySelector<CrActionMenuElement>(
                                   '#menu')!.open);
    const items =
        testElement.shadowRoot!.querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(3, items.length);
    assertTrue(items[0]!.textContent.includes('foo@foo.com'));
    assertTrue(items[1]!.textContent.includes('bar@bar.com'));
    assertEquals(items[2]!.id, 'sign-in-item');

    // "sync to" button is showing the correct name and syncs with the
    // correct account when clicked.
    assertTrue(isVisible(syncButton));
    assertFalse(isChildVisible(testElement, '#turn-off'));
    syncButton.click();
    flush();

    let [email, isDefaultPromoAccount] =
        await browserProxy.whenCalled('startSyncingWithEmail');
    assertEquals(email, 'foo@foo.com');
    assertEquals(isDefaultPromoAccount, true);

    assertTrue(isChildVisible(testElement, 'cr-icon-button'));
    assertTrue(testElement.shadowRoot!
                   .querySelector<HTMLElement>('#sync-icon-container')!.hidden);

    assertTrue(isChildVisible(testElement, '#dropdown-arrow'));
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#dropdown-arrow')!.click();
    flush();
    assertTrue(
        testElement.shadowRoot!.querySelector<CrActionMenuElement>(
                                   '#menu')!.open);

    // Switching selected account will update UI with the right name and
    // email.
    items[1]!.click();
    flush();
    assertFalse(userInfo.textContent.includes('fooName'));
    assertFalse(userInfo.textContent.includes('foo@foo.com'));
    assertTrue(userInfo.textContent.includes('barName'));
    assertTrue(userInfo.textContent.includes('bar@bar.com'));
    assertTrue(isVisible(syncButton));

    browserProxy.resetResolver('startSyncingWithEmail');
    syncButton.click();
    flush();

    [email, isDefaultPromoAccount] =
        await browserProxy.whenCalled('startSyncingWithEmail');
    assertEquals(email, 'bar@bar.com');
    assertEquals(isDefaultPromoAccount, false);

    // Tapping the last menu item will initiate sign-in.
    items[2]!.click();
    await browserProxy.whenCalled('startSignIn');
  });

  test(
      'sync button not visible with replaceSyncPromosWithSignInPromos',
      function() {
        loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});
        resetRouterForTesting();
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        testElement = document.createElement('settings-sync-account-control');
        testElement.syncStatus = {
          signedInState: SignedInState.SIGNED_IN,
          statusAction: StatusAction.NO_ACTION,
        };
        document.body.appendChild(testElement);
        flush();

        assertFalse(isChildVisible(testElement, '#sync-button'));
      });

  test(
      'Updated UI shown when sync off', function() {
        testElement.syncStatus = {
          signedInState: SignedInState.SIGNED_IN,
          statusAction: StatusAction.NO_ACTION,
        };

        flush();

        assertTrue(isChildVisible(testElement, '#sync-button'));
        assertFalse(isChildVisible(testElement, '#dropdown-arrow'));
        assertTrue(isChildVisible(testElement, '#signout-button'));
      });

  test('Signout buttons not available to managed accounts', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
      domain: 'domain',
    };

    flush();

    assertTrue(isChildVisible(testElement, '#sync-button'));
    assertFalse(isChildVisible(testElement, '#signout-button'));
    assertFalse(isChildVisible(testElement, '#remove-account-button'));

    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_IN_PAUSED,
      statusAction: StatusAction.NO_ACTION,
      domain: 'domain',
    };
    assertFalse(isChildVisible(testElement, '#signout-button'));
    assertFalse(isChildVisible(testElement, '#remove-account-button'));
  });

  test('managedUser, Sync off, turn sync off enabled', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      disabled: false,
      hasError: false,
      domain: 'domain',
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(isChildVisible(testElement, '#sync-button'));
    // Menu is hidden.
    assertFalse(!!testElement.shadowRoot!.querySelector('#menu'));
    assertFalse(isChildVisible(testElement, '#dropdown-arrow'));
  });

  test('signed in, no error', function() {
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
      hasUnrecoverableError: false,
      disabled: false,
    };
    flush();

    assertTrue(isChildVisible(testElement, '#avatar-row'));
    assertFalse(isChildVisible(testElement, '#promo-header'));
    assertFalse(
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#sync-icon-container')!.hidden);

    assertFalse(isChildVisible(testElement, 'cr-icon-button'));
    assertFalse(!!testElement.shadowRoot!.querySelector('#menu'));
    assertFalse(isChildVisible(testElement, '#dropdown-arrow'));

    const userInfo =
        testElement.shadowRoot!.querySelector<HTMLElement>('#user-info')!;
    assertTrue(userInfo.textContent.includes('barName'));
    assertTrue(userInfo.textContent.includes('bar@bar.com'));
    assertFalse(userInfo.textContent.includes('fooName'));
    assertFalse(userInfo.textContent.includes('foo@foo.com'));

    assertFalse(isChildVisible(testElement, '#sync-button'));
    assertTrue(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#avatar-row #turn-off')!.click();
    flush();

    assertEquals(
        Router.getInstance().getCurrentRoute(),
        Router.getInstance().getRoutes().SIGN_OUT);
  });


  test(
      'signed in, has error', function() {
        testElement.syncStatus = {
          firstSetupInProgress: false,
          signedInState: SignedInState.SYNCING,
          hasError: true,
          hasUnrecoverableError: false,
          statusAction: StatusAction.CONFIRM_SYNC_SETTINGS,
          statusText: 'error text',
          disabled: false,
        };
        flush();
        const userInfo = testElement.shadowRoot!.querySelector('#user-info')!;

        assertTrue(testElement.shadowRoot!
                       .querySelector<HTMLElement>('#sync-icon-container')!
                       .classList.contains('sync-problem'));
        assertTrue(!!testElement.shadowRoot!.querySelector(
            '[icon="settings:sync-problem"]'));
        const displayedText =
            userInfo.querySelector<HTMLElement>(
                        'div:not([hidden])')!.textContent;
        assertTrue(displayedText.includes('fooName'));
        assertTrue(isChildVisible(testElement, '#sync-error-button'));
        assertTrue(isChildVisible(testElement, '#turn-off'));
        assertFalse(
            isVisible(testElement.shadowRoot!.querySelector('#banner')));
      });


  test(
      'signed in, has passphrase error', function() {
        testElement.syncStatus = {
          firstSetupInProgress: false,
          signedInState: SignedInState.SIGNED_IN,
          hasError: true,
          statusAction: StatusAction.ENTER_PASSPHRASE,
          statusText: 'error text',
          disabled: false,
        };
        flush();

        assertTrue(testElement.shadowRoot!
                       .querySelector<HTMLElement>('#sync-icon-container')!
                       .classList.contains('sync-problem'));
        assertTrue(!!testElement.shadowRoot!.querySelector(
            '[icon="settings:sync-problem"]'));
        assertTrue(isChildVisible(testElement, '#sync-error-button'));
        assertTrue(isChildVisible(testElement, '#turn-off'));
        assertFalse(
            isVisible(testElement.shadowRoot!.querySelector('#banner')));
      });

  test(
      'user in sync paused state', function() {
        testElement.syncStatus = {
          firstSetupInProgress: false,
          signedInState: SignedInState.SYNCING,
          signedInUsername: 'bar@bar.com',
          hasError: true,
          hasUnrecoverableError: false,
          statusAction: StatusAction.REAUTHENTICATE,
          disabled: false,
        };

        const userInfo = testElement.shadowRoot!.querySelector('#user-info')!;
        const displayedText =
            userInfo.querySelector<HTMLElement>(
                        'div:not([hidden])')!.textContent;

        assertTrue(
            testElement.shadowRoot!
                .querySelector<HTMLElement>(
                    '#sync-icon-container')!.classList.contains('sync-paused'));
        assertTrue(!!testElement.shadowRoot!.querySelector(
            '[icon=\'settings:sync-disabled\']'));
        assertFalse(displayedText.includes('barName'));
        assertFalse(displayedText.includes('fooName'));
        assertTrue(displayedText.includes('Sync is paused'));
        // The sync error button is shown to resolve the error.
        assertTrue(isChildVisible(testElement, '#sync-error-button'));
      });


  test('signed in, setup in progress', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.NO_ACTION,
      statusText: 'Setup in progress...',
      firstSetupInProgress: true,
      hasError: false,
      hasUnrecoverableError: false,
      disabled: false,
    };
    flush();
    const userInfo = testElement.shadowRoot!.querySelector('#user-info')!;
    const setupButtons =
        testElement.shadowRoot!.querySelector('#setup-buttons');

    assertTrue(userInfo.textContent.includes('barName'));
    assertTrue(userInfo.textContent.includes('Setup in progress...'));
    assertTrue(isVisible(setupButtons));
  });

  test('signed in, setup in progress with error', function() {
    testElement.embeddedInSubpage = true;

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.REAUTHENTICATE,
      statusText: 'Sign in again',
      firstSetupInProgress: true,
      hasError: true,
      hasUnrecoverableError: false,
      disabled: false,
    };
    flush();
    const userInfo = testElement.shadowRoot!.querySelector('#user-info')!;
    const setupButtons =
        testElement.shadowRoot!.querySelector('#setup-buttons');

    assertTrue(userInfo.textContent.includes('Sign in again'));
    assertTrue(isVisible(setupButtons));

    // Other buttons are not shown, https://crbug.com/405980868
    assertFalse(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));
    assertFalse(isChildVisible(testElement, '#signin-paused-buttons'));
    assertFalse(isChildVisible(testElement, '#sync-button'));
    assertFalse(isChildVisible(testElement, '#signout-button'));
    assertFalse(isChildVisible(testElement, '#account-aware'));
  });

  test('embedded in another page', function() {
    testElement.embeddedInSubpage = true;

    // Force promo reset
    const sync_state = SignedInState.SIGNED_OUT;
    const opposite_syncing = SignedInState.SYNCING;

    testElement.syncStatus = {
      signedInState: opposite_syncing,
      statusAction: StatusAction.NO_ACTION,
    };
    testElement.syncStatus = {
      signedInState: sync_state,
      statusAction: StatusAction.NO_ACTION,
    };

    const banner = testElement.shadowRoot!.querySelector('#banner');
    assertTrue(isVisible(banner));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
      hasUnrecoverableError: false,
      disabled: false,
    };

    assertTrue(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.embeddedInSubpage = true;
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: false,
      statusAction: StatusAction.REAUTHENTICATE,
      disabled: false,
    };
    assertTrue(isChildVisible(testElement, '#turn-off'));
    assertTrue(isChildVisible(testElement, '#sync-error-button'));

    testElement.embeddedInSubpage = true;
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: true,
      statusAction: StatusAction.REAUTHENTICATE,
      disabled: false,
    };
    assertTrue(isChildVisible(testElement, '#turn-off'));
    assertTrue(isChildVisible(testElement, '#sync-error-button'));

    testElement.embeddedInSubpage = true;
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: false,
      statusAction: StatusAction.ENTER_PASSPHRASE,
      disabled: false,
    };
    assertTrue(isChildVisible(testElement, '#turn-off'));
    // Don't show passphrase error button on embedded page.
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.embeddedInSubpage = true;
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: true,
      statusAction: StatusAction.NO_ACTION,
      disabled: false,
    };
    assertTrue(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));
  });

  test('hide buttons', function() {
    testElement.hideButtons = true;
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
      hasUnrecoverableError: false,
      disabled: false,
    };

    assertFalse(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: false,
      statusAction: StatusAction.REAUTHENTICATE,
      disabled: false,
    };
    assertFalse(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: false,
      statusAction: StatusAction.ENTER_PASSPHRASE,
      disabled: false,
    };
    assertFalse(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));
  });

  test('signinButtonDisabled', function() {
    // Ensure that the sync button is disabled when signin is disabled.
    assertFalse(testElement.$.signIn.disabled);
    testElement.setPrefValue('signin.allowed_on_next_startup', false);
    flush();
    assertTrue(testElement.$.signIn.disabled);
  });

  test('signinPaused effects', function() {
    const signedInAccount: StoredAccount = {
      fullName: 'fooName',
      givenName: 'foo',
      email: 'foo@foo.com',
      isPrimaryAccount: true,
    };
    // Set primary account.
    simulateStoredAccounts([signedInAccount]);

    // Signed in but not syncing.
    testElement.syncStatus = {
      statusAction: StatusAction.NO_ACTION,
      signedInState: SignedInState.SIGNED_IN,
    };

    assertTrue(isChildVisible(testElement, '#avatar-row'));
    const userInfo =
        testElement.shadowRoot!.querySelector<HTMLElement>('#user-info')!;
    const secondaryContentSignedIn = userInfo.children[1]!.textContent;
    assertNotEquals(secondaryContentSignedIn.trim(), signedInAccount.email);
    assertFalse(isChildVisible(testElement, '#signin-paused-buttons'));
    assertFalse(isChildVisible(testElement, '#dropdown-arrow'));
    assertTrue(isChildVisible(testElement, '#sync-button'));

    // Set Signed in Paused state.
    testElement.syncStatus = {
      statusAction: StatusAction.NO_ACTION,
      signedInState: SignedInState.SIGNED_IN_PAUSED,
    };

    assertTrue(isChildVisible(testElement, '#avatar-row'));
    const secondaryContentSigninPaused = userInfo.children[1]!.textContent;
    assertNotEquals(secondaryContentSignedIn, secondaryContentSigninPaused);
    assertEquals(secondaryContentSigninPaused.trim(), signedInAccount.email);
    assertTrue(isChildVisible(testElement, '#signin-paused-buttons'));
    assertTrue(isChildVisible(testElement, '#remove-account-button'));
    assertFalse(isChildVisible(testElement, '#dropdown-arrow'));
    assertFalse(isChildVisible(testElement, '#sync-button'));
  });

  test(
      'signed out with account awareness', async function() {
        testElement.syncStatus = {
          firstSetupInProgress: false,
          signedInState: SignedInState.WEB_ONLY_SIGNED_IN,
          signedInUsername: 'foo@foo.com',
          hasError: false,
          hasUnrecoverableError: false,
          statusAction: StatusAction.REAUTHENTICATE,
          disabled: false,
        };

        assertTrue(isVisible(testElement.shadowRoot!.querySelector('#banner')));
        assertTrue(isChildVisible(testElement, '#dropdown-arrow'));

        const continueAsButton =
            testElement.shadowRoot!.querySelector<HTMLElement>(
                '#account-aware')!;
        assertFalse(continueAsButton.hidden);
        continueAsButton.click();

        const [email, isDefaultPromoAccount] =
            await browserProxy.whenCalled('startSyncingWithEmail');

        assertEquals(email, 'foo@foo.com');
        assertEquals(isDefaultPromoAccount, true);
      });


  test(
      'sync off has passphrase error', async function() {
        testElement.syncStatus = {
          firstSetupInProgress: false,
          signedInState: SignedInState.SIGNED_IN,
          signedInUsername: 'foo@foo.com',
          hasError: true,
          statusAction: StatusAction.ENTER_PASSPHRASE,
        };

        assertTrue(isChildVisible(testElement, '#sync-error-button'));
        const signOut =
            testElement.shadowRoot!.querySelector<HTMLElement>('#turn-off')!;
        assertFalse(signOut.hidden);
        signOut.click();
        const deleteProfile = await browserProxy.whenCalled('signOut');
        assertFalse(deleteProfile);
      });
});

suite('SyncAccountControlHideBanner', function() {
  let browserProxy: TestSyncBrowserProxy;
  let testElement: SettingsSyncAccountControlElement;

  setup(async function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = Object.assign(
        document.createElement('settings-sync-account-control'),
        {hideBanner: true});

    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    };
    testElement.prefs = {
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
    };

    document.body.appendChild(testElement);

    await browserProxy.whenCalled('getStoredAccounts');
    flush();
    simulateStoredAccounts([]);
  });

  teardown(function() {
    testElement.remove();
  });

  test('hide banner', function() {
    assertFalse(isVisible(testElement.shadowRoot!.querySelector('#banner')));
  });
});
