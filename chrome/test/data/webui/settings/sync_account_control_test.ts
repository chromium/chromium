// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="not chromeos_ash">
import {CrActionMenuElement} from 'chrome://settings/settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>

import {MAX_SIGNIN_PROMO_IMPRESSION, Router, SettingsSyncAccountControlElement, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {setupRouterWithSyncRoutes, simulateStoredAccounts, SyncRoutes} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on


suite('SyncAccountControl', function() {
  let browserProxy: TestSyncBrowserProxy;
  let testElement: SettingsSyncAccountControlElement;

  function forcePromoResetWithCount(count: number, signedIn: boolean) {
    browserProxy.setImpressionCount(count);
    // Flipping syncStatus.signedIn will force promo state to be reset.
    testElement.syncStatus = {
      signedIn: !signedIn,
      statusAction: StatusAction.NO_ACTION,
    };
    testElement.syncStatus = {
      signedIn: signedIn,
      statusAction: StatusAction.NO_ACTION,
    };
  }

  setup(async function() {
    setupRouterWithSyncRoutes();
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-sync-account-control');
    testElement.syncStatus = {
      signedIn: true,
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

  test('promo shows/hides in the right states', async function() {
    // Not signed in, no accounts, will show banner.
    simulateStoredAccounts([]);
    forcePromoResetWithCount(0, false);
    const banner = testElement.shadowRoot!.querySelector('#banner');
    assertTrue(isVisible(banner));
    // Flipping signedIn in forcePromoResetWithCount should increment count.
    await browserProxy.whenCalled('incrementPromoImpressionCount');
    forcePromoResetWithCount(MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
    assertFalse(isVisible(banner));

    // Not signed in, has accounts, will show banner.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    forcePromoResetWithCount(0, false);
    assertTrue(isVisible(banner));
    forcePromoResetWithCount(MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
    assertFalse(isVisible(banner));

    // signed in, banners never show.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    forcePromoResetWithCount(0, true);
    assertFalse(isVisible(banner));
    forcePromoResetWithCount(MAX_SIGNIN_PROMO_IMPRESSION + 1, true);
    assertFalse(isVisible(banner));
  });

  test('promo header is visible', function() {
    testElement.syncStatus = {
      signedIn: false,
      signedInUsername: '',
      statusAction: StatusAction.NO_ACTION,
    };
    testElement.promoLabelWithNoAccount = testElement.promoLabelWithAccount =
        'title';
    simulateStoredAccounts([]);
    assertTrue(isChildVisible(testElement, '#promo-header'));
  });

  test('not signed in and no stored accounts', async function() {
    testElement.syncStatus = {
      signedIn: false,
      signedInUsername: '',
      statusAction: StatusAction.NO_ACTION,
    };
    simulateStoredAccounts([]);

    assertTrue(isChildVisible(testElement, '#promo-header'));
    assertFalse(isChildVisible(testElement, '#avatar-row'));

    // <if expr="not chromeos_ash">
    // Chrome OS does not use the account switch menu.
    assertFalse(isChildVisible(testElement, '#menu'));
    // </if>

    assertTrue(isChildVisible(testElement, '#signIn'));

    testElement.$.signIn.click();

    // <if expr="chromeos_ash">
    await browserProxy.whenCalled('turnOnSync');
    // </if>

    // <if expr="not chromeos_ash">
    await browserProxy.whenCalled('startSignIn');
    // </if>
  });

  // <if expr="not chromeos_ash">
  // Chrome OS users are always signed in.
  test('not signed in but has stored accounts', async function() {
    loadTimeData.overrideValues({isSecondaryUser: true});
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: false,
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
    assertTrue(userInfo.textContent!.includes('fooName'));
    assertTrue(userInfo.textContent!.includes('foo@foo.com'));
    assertFalse(userInfo.textContent!.includes('barName'));
    assertFalse(userInfo.textContent!.includes('bar@bar.com'));

    // Menu contains the right items.
    assertTrue(!!testElement.shadowRoot!.querySelector('#menu'));
    assertFalse(
        testElement.shadowRoot!.querySelector<CrActionMenuElement>(
                                   '#menu')!.open);
    const items =
        testElement.shadowRoot!.querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(4, items.length);
    assertTrue(items[0]!.textContent!.includes('foo@foo.com'));
    assertTrue(items[1]!.textContent!.includes('bar@bar.com'));
    assertEquals(items[2]!.id, 'sign-in-item');
    assertEquals(items[3]!.id, 'sign-out-item');

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
    assertFalse(userInfo.textContent!.includes('fooName'));
    assertFalse(userInfo.textContent!.includes('foo@foo.com'));
    assertTrue(userInfo.textContent!.includes('barName'));
    assertTrue(userInfo.textContent!.includes('bar@bar.com'));
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
  // </if>

  // <if expr="chromeos_lacros">
  test('main profile not signed in but has stored accounts', function() {
    loadTimeData.overrideValues({isSecondaryUser: false});
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: false,
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
    ]);

    const userInfo =
        testElement.shadowRoot!.querySelector<HTMLElement>('#user-info')!;

    // Avatar row shows the right account.
    assertTrue(isChildVisible(testElement, '#promo-header'));
    assertTrue(isChildVisible(testElement, '#avatar-row'));
    assertTrue(userInfo.textContent!.includes('fooName'));
    assertTrue(userInfo.textContent!.includes('foo@foo.com'));

    // Menu is hidden.
    assertFalse(!!testElement.shadowRoot!.querySelector('#menu'));
    assertFalse(isChildVisible(testElement, '#dropdown-arrow'));
  });
  // </if>

  test('signed in, no error', function() {
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
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

    // <if expr="not chromeos_ash">
    // Chrome OS does not use the account switch menu.
    assertFalse(isChildVisible(testElement, 'cr-icon-button'));
    assertFalse(!!testElement.shadowRoot!.querySelector('#menu'));
    assertFalse(isChildVisible(testElement, '#dropdown-arrow'));
    // </if>

    const userInfo =
        testElement.shadowRoot!.querySelector<HTMLElement>('#user-info')!;
    assertTrue(userInfo.textContent!.includes('barName'));
    assertTrue(userInfo.textContent!.includes('bar@bar.com'));
    assertFalse(userInfo.textContent!.includes('fooName'));
    assertFalse(userInfo.textContent!.includes('foo@foo.com'));

    assertFalse(isChildVisible(testElement, '#sync-button'));
    assertTrue(isChildVisible(testElement, '#turn-off'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#avatar-row #turn-off')!.click();
    flush();

    assertEquals(
        Router.getInstance().getCurrentRoute(),
        (Router.getInstance().getRoutes() as SyncRoutes).SIGN_OUT);
  });

  test('signed in, has error', function() {
    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: false,
      statusAction: StatusAction.CONFIRM_SYNC_SETTINGS,
      disabled: false,
    };
    flush();
    const userInfo = testElement.shadowRoot!.querySelector('#user-info')!;

    assertTrue(
        testElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#sync-icon-container')!.classList.contains('sync-problem'));
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '[icon="settings:sync-problem"]'));
    let displayedText =
        userInfo.querySelector<HTMLElement>('div:not([hidden])')!.textContent!;
    assertFalse(displayedText.includes('barName'));
    assertFalse(displayedText.includes('fooName'));
    assertTrue(displayedText.includes('Sync isn\'t working'));
    // The sync error button is shown to resolve the error.
    assertTrue(isChildVisible(testElement, '#sync-error-button'));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
      signedInUsername: 'bar@bar.com',
      hasError: true,
      hasUnrecoverableError: false,
      statusAction: StatusAction.REAUTHENTICATE,
      disabled: false,
    };
    assertTrue(
        testElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#sync-icon-container')!.classList.contains('sync-paused'));
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '[icon=\'settings:sync-disabled\']'));
    displayedText =
        userInfo.querySelector<HTMLElement>('div:not([hidden])')!.textContent!;
    assertFalse(displayedText.includes('barName'));
    assertFalse(displayedText.includes('fooName'));
    assertTrue(displayedText.includes('Sync is paused'));
    // The sync error button is shown to resolve the error.
    assertTrue(isChildVisible(testElement, '#sync-error-button'));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
      hasUnrecoverableError: false,
      disabled: true,
    };

    assertTrue(
        testElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#sync-icon-container')!.classList.contains('sync-disabled'));
    assertTrue(!!testElement.shadowRoot!.querySelector('[icon=\'cr:sync\']'));
    displayedText =
        userInfo.querySelector<HTMLElement>('div:not([hidden])')!.textContent!;
    assertFalse(displayedText.includes('barName'));
    assertFalse(displayedText.includes('fooName'));
    assertTrue(displayedText.includes('Sync disabled'));
    assertFalse(isChildVisible(testElement, '#sync-error-button'));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.REAUTHENTICATE,
      hasError: true,
      hasUnrecoverableError: true,
      disabled: false,
    };
    assertTrue(
        testElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#sync-icon-container')!.classList.contains('sync-problem'));
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '[icon="settings:sync-problem"]'));
    displayedText =
        userInfo.querySelector<HTMLElement>('div:not([hidden])')!.textContent!;
    assertFalse(displayedText.includes('barName'));
    assertFalse(displayedText.includes('fooName'));
    assertTrue(displayedText.includes('Sync isn\'t working'));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
      signedInUsername: 'bar@bar.com',
      statusAction: StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS,
      hasError: true,
      hasPasswordsOnlyError: true,
      hasUnrecoverableError: false,
      disabled: false,
    };
    assertTrue(
        testElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#sync-icon-container')!.classList.contains('sync-problem'));
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '[icon="settings:sync-problem"]'));
    displayedText =
        userInfo.querySelector<HTMLElement>('div:not([hidden])')!.textContent!;
    assertFalse(displayedText.includes('barName'));
    assertFalse(displayedText.includes('fooName'));
    assertFalse(displayedText.includes('Sync isn\'t working'));
    assertTrue(displayedText.includes('Password sync isn\'t working'));
    // The sync error button is shown to resolve the error.
    assertTrue(isChildVisible(testElement, '#sync-error-button'));
    assertTrue(isChildVisible(testElement, '#turn-off'));
  });

  test('signed in, setup in progress', function() {
    testElement.syncStatus = {
      signedIn: true,
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

    assertTrue(userInfo.textContent!.includes('barName'));
    assertTrue(userInfo.textContent!.includes('Setup in progress...'));
    assertTrue(isVisible(setupButtons));
  });

  test('embedded in another page', function() {
    testElement.embeddedInSubpage = true;
    forcePromoResetWithCount(100, false);
    const banner = testElement.shadowRoot!.querySelector('#banner');
    assertTrue(isVisible(banner));

    testElement.syncStatus = {
      firstSetupInProgress: false,
      signedIn: true,
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
      signedIn: true,
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
      signedIn: true,
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
      signedIn: true,
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
      signedIn: true,
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
      signedIn: true,
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
      signedIn: true,
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
      signedIn: true,
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
});
