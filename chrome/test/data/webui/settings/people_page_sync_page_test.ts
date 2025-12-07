// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrExpandButtonElement, CrInputElement, SettingsSyncEncryptionOptionsElement, SettingsSyncPageElement} from 'chrome://settings/lazy_load.js';
// <if expr="not is_chromeos">
import type {CrDialogElement} from 'chrome://settings/lazy_load.js';
// </if>
import type {CrCollapseElement} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement, CrRadioButtonElement, CrRadioGroupElement} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl} from 'chrome://settings/settings.js';
import {loadTimeData, OpenWindowProxyImpl, PageStatus, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible, eventToPromise} from 'chrome://webui-test/test_util.js';

// <if expr="not is_chromeos">
import {simulateStoredAccounts} from './sync_test_util.js';
import {resetRouterForTesting} from 'chrome://settings/settings.js';
// </if>

import {getSyncAllPrefs} from './sync_test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('SyncSettings', function() {
  let syncPage: SettingsSyncPageElement;
  let browserProxy: TestSyncBrowserProxy;
  let encryptionElement: SettingsSyncEncryptionOptionsElement;
  let encryptionRadioGroup: CrRadioGroupElement;
  let encryptWithGoogle: CrRadioButtonElement;
  let encryptWithPassphrase: CrRadioButtonElement;

  function setupSyncPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    syncPage = document.createElement('settings-sync-page');
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SYNC);
    // Preferences should exist for embedded
    // 'personalization_options.html'. We don't perform tests on them.
    syncPage.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
    };

    document.body.appendChild(syncPage);

    webUIListenerCallback('page-status-changed', PageStatus.CONFIGURE);
    assertFalse(
        syncPage.shadowRoot!
            .querySelector<HTMLElement>('#' + PageStatus.CONFIGURE)!.hidden);
    assertTrue(
        syncPage.shadowRoot!
            .querySelector<HTMLElement>('#' + PageStatus.SPINNER)!.hidden);

    // Start with Sync All with no encryption selected. Also, ensure
    // that this is not a supervised user, so that Sync Passphrase is
    // enabled.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    syncPage.set('syncStatus', {
      signedInState: SignedInState.SYNCING,
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({signinAllowed: true});
  });

  setup(async function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    setupSyncPage();

    await waitBeforeNextRender(syncPage);
    encryptionElement =
        syncPage.shadowRoot!.querySelector('settings-sync-encryption-options')!;
    assertTrue(!!encryptionElement);
    encryptionRadioGroup =
        encryptionElement.shadowRoot!.querySelector('#encryptionRadioGroup')!;
    encryptWithGoogle = encryptionElement.shadowRoot!.querySelector(
        'cr-radio-button[name="encrypt-with-google"]')!;
    encryptWithPassphrase = encryptionElement.shadowRoot!.querySelector(
        'cr-radio-button[name="encrypt-with-passphrase"]')!;
    assertTrue(!!encryptionRadioGroup);
    assertTrue(!!encryptWithGoogle);
    assertTrue(!!encryptWithPassphrase);
  });

  teardown(function() {
    syncPage.remove();
  });

  // #######################
  // TESTS FOR ALL PLATFORMS
  // #######################

  test('NotifiesHandlerOfNavigation', async function() {
    await browserProxy.whenCalled('didNavigateToSyncPage');

    // Navigate away.
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().PEOPLE);
    await browserProxy.whenCalled('didNavigateAwayFromSyncPage');

    // Navigate back to the page.
    browserProxy.resetResolver('didNavigateToSyncPage');
    router.navigateTo(router.getRoutes().SYNC);
    await browserProxy.whenCalled('didNavigateToSyncPage');

    // Remove page element.
    browserProxy.resetResolver('didNavigateAwayFromSyncPage');
    syncPage.remove();
    await browserProxy.whenCalled('didNavigateAwayFromSyncPage');

    // Recreate page element.
    browserProxy.resetResolver('didNavigateToSyncPage');
    syncPage = document.createElement('settings-sync-page');
    router.navigateTo(router.getRoutes().SYNC);
    document.body.appendChild(syncPage);
    await browserProxy.whenCalled('didNavigateToSyncPage');
  });

  test('SyncSectionLayout_SignedIn', function() {
    const syncSection =
        syncPage.shadowRoot!.querySelector<HTMLElement>('#sync-section')!;
    const otherItems =
        syncPage.shadowRoot!.querySelector<HTMLElement>('#other-sync-items')!;

    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SYNCING,
      disabled: false,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(syncSection.hidden);
    assertTrue(
        syncPage.shadowRoot!.querySelector<HTMLElement>(
                                '#sync-separator')!.hidden);
    assertTrue(otherItems.classList.contains('list-frame'));
    assertEquals(otherItems.querySelectorAll('cr-expand-button').length, 1);

    assertTrue(isChildVisible(syncPage, '#sync-advanced-row'));
    assertTrue(isChildVisible(syncPage, '#activityControlsLinkRowV2'));
    assertFalse(isChildVisible(syncPage, '#personalizationExpandButton'));
    assertTrue(isChildVisible(syncPage, '#syncDashboardLink'));

    // Test sync paused state.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SYNCING,
      disabled: false,
      hasError: true,
      statusAction: StatusAction.REAUTHENTICATE,
    });
    assertTrue(syncSection.hidden);
    assertFalse(
        syncPage.shadowRoot!.querySelector<HTMLElement>(
                                '#sync-separator')!.hidden);

    // Test passphrase error state.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SYNCING,
      disabled: false,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    assertFalse(syncSection.hidden);
    assertTrue(
        syncPage.shadowRoot!.querySelector<HTMLElement>(
                                '#sync-separator')!.hidden);
  });

  test('SyncSectionLayout_SignedOut', function() {
    const syncSection =
        syncPage.shadowRoot!.querySelector<HTMLElement>('#sync-section')!;

    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_OUT,
      disabled: false,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertTrue(syncSection.hidden);
    assertFalse(
        syncPage.shadowRoot!.querySelector<HTMLElement>(
                                '#sync-separator')!.hidden);
  });

  test('SyncSectionLayout_SyncDisabled', function() {
    const syncSection =
        syncPage.shadowRoot!.querySelector<HTMLElement>('#sync-section')!;

    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_IN,
      disabled: true,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertTrue(syncSection.hidden);
  });

  test('LoadingAndTimeout', function() {
    const configurePage = syncPage.shadowRoot!.querySelector<HTMLElement>(
        '#' + PageStatus.CONFIGURE)!;
    const spinnerPage = syncPage.shadowRoot!.querySelector<HTMLElement>(
        '#' + PageStatus.SPINNER)!;

    // NOTE: This isn't called in production, but the test suite starts the
    // tests with PageStatus.CONFIGURE.
    webUIListenerCallback('page-status-changed', PageStatus.SPINNER);
    assertTrue(configurePage.hidden);
    assertFalse(spinnerPage.hidden);

    webUIListenerCallback('page-status-changed', PageStatus.CONFIGURE);
    assertFalse(configurePage.hidden);
    assertTrue(spinnerPage.hidden);

    // Should remain on the CONFIGURE page even if the passphrase failed.
    webUIListenerCallback('page-status-changed', PageStatus.PASSPHRASE_FAILED);
    assertFalse(configurePage.hidden);
    assertTrue(spinnerPage.hidden);
  });

  test('EncryptionExpandButton', async function() {
    const encryptionDescription =
        syncPage.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#encryptionDescription');
    assertTrue(!!encryptionDescription);
    const encryptionCollapse = syncPage.$.encryptionCollapse;

    // No encryption with custom passphrase.
    assertFalse(encryptionCollapse.opened);
    encryptionDescription.click();
    await encryptionDescription.updateComplete;
    assertTrue(encryptionCollapse.opened);

    // Push sync prefs with |prefs.encryptAllData| unchanged. The encryption
    // menu should not collapse.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    flush();
    assertTrue(encryptionCollapse.opened);

    encryptionDescription.click();
    await encryptionDescription.updateComplete;
    assertFalse(encryptionCollapse.opened);

    // Data encrypted with custom passphrase.
    // The encryption menu should be expanded.
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();
    assertTrue(encryptionCollapse.opened);

    // Clicking |reset Sync| does not change the expansion state.
    const link =
        encryptionDescription.querySelector<HTMLAnchorElement>('a[href]');
    assertTrue(!!link);
    link.target = '';
    link.href = '#';
    // Prevent the link from triggering a page navigation when tapped.
    // Breaks the test in Vulcanized mode.
    link.addEventListener('click', e => e.preventDefault());
    link.click();
    assertTrue(encryptionCollapse.opened);
  });

  test('RadioBoxesHiddenWhenPassphraseRequired', function() {
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);

    flush();

    assertTrue(
        syncPage.shadowRoot!
            .querySelector<HTMLElement>('#encryptionDescription')!.hidden);
    assertEquals(
        encryptionElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#encryptionRadioGroupContainer')!.style.display,
        'none');
  });

  test('EnterPassphraseLabelWhenNoPassphraseTime', () => {
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();
    const enterPassphraseLabel =
        syncPage.shadowRoot!.querySelector<HTMLElement>(
            '#enterPassphraseLabel')!;

    assertEquals(
        'Your data is encrypted with your sync passphrase. Enter it to start' +
            ' sync.',
        enterPassphraseLabel.innerText);
  });

  test('EnterPassphraseLabelWhenHasPassphraseTime', () => {
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    prefs.explicitPassphraseTime = 'Jan 01, 1970';
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();
    const enterPassphraseLabel =
        syncPage.shadowRoot!.querySelector<HTMLElement>(
            '#enterPassphraseLabel')!;

    assertEquals(
        `Your data was encrypted with your sync passphrase on ${
            prefs.explicitPassphraseTime}. Enter it to start sync.`,
        enterPassphraseLabel.innerText);
  });

  test(
      'ExistingPassphraseSubmitButtonDisabledWhenExistingPassphraseEmpty',
      async () => {
        const prefs = getSyncAllPrefs();
        prefs.encryptAllData = true;
        prefs.passphraseRequired = true;
        webUIListenerCallback('sync-prefs-changed', prefs);
        flush();

        const existingPassphraseInput =
            syncPage.shadowRoot!.querySelector<CrInputElement>(
                '#existingPassphraseInput');
        assertTrue(!!existingPassphraseInput);
        const submitExistingPassphrase =
            syncPage.shadowRoot!.querySelector<CrButtonElement>(
                '#submitExistingPassphrase')!;

        existingPassphraseInput.value = '';
        await existingPassphraseInput.updateComplete;
        assertTrue(submitExistingPassphrase.disabled);

        existingPassphraseInput.value = 'foo';
        await existingPassphraseInput.updateComplete;
        assertFalse(submitExistingPassphrase.disabled);
      });

  test('EnterExistingWrongPassphrase', async function() {
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    const existingPassphraseInput =
        syncPage.shadowRoot!.querySelector<CrInputElement>(
            '#existingPassphraseInput');
    assertTrue(!!existingPassphraseInput);
    existingPassphraseInput.value = 'wrong';
    browserProxy.decryptionPassphraseSuccess = false;

    const submitExistingPassphrase =
        syncPage.shadowRoot!.querySelector<CrButtonElement>(
            '#submitExistingPassphrase');
    assertTrue(!!submitExistingPassphrase);
    await existingPassphraseInput.updateComplete;
    submitExistingPassphrase.click();

    const passphrase = await browserProxy.whenCalled('setDecryptionPassphrase');

    assertEquals('wrong', passphrase);
    assertTrue(existingPassphraseInput.invalid);
  });

  test('EnterExistingCorrectPassphrase', async function() {
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    const existingPassphraseInput =
        syncPage.shadowRoot!.querySelector<CrInputElement>(
            '#existingPassphraseInput');
    assertTrue(!!existingPassphraseInput);
    existingPassphraseInput.value = 'right';
    browserProxy.decryptionPassphraseSuccess = true;

    const submitExistingPassphrase =
        syncPage.shadowRoot!.querySelector<CrButtonElement>(
            '#submitExistingPassphrase');
    assertTrue(!!submitExistingPassphrase);
    await existingPassphraseInput.updateComplete;
    submitExistingPassphrase.click();

    const passphrase = await browserProxy.whenCalled('setDecryptionPassphrase');

    assertEquals('right', passphrase);

    // Fake backend response.
    const newPrefs = getSyncAllPrefs();
    newPrefs.encryptAllData = true;
    webUIListenerCallback('sync-prefs-changed', newPrefs);

    flush();
    await eventToPromise('selected-changed', encryptionRadioGroup);

    // Verify that the encryption radio boxes are shown but disabled.
    assertTrue(encryptionRadioGroup.disabled);
    assertEquals(-1, encryptWithGoogle.$.button.tabIndex);
    assertEquals(-1, encryptWithPassphrase.$.button.tabIndex);

    // Confirm that the page navigates away form the sync setup.
    await browserProxy.whenCalled('didNavigateAwayFromSyncPage');
    const router = Router.getInstance();
    assertEquals(router.getRoutes().PEOPLE, router.getCurrentRoute());
  });

  test('EnterExistingPassphraseDoesNotExistIfSignedOut', function() {
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_IN,
      disabled: false,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });

    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertFalse(!!syncPage.shadowRoot!.querySelector<CrInputElement>(
        '#existingPassphraseInput'));
  });

  test('SyncAdvancedRow', function() {
    flush();

    const syncAdvancedRow =
        syncPage.shadowRoot!.querySelector<HTMLElement>('#sync-advanced-row')!;
    assertFalse(syncAdvancedRow.hidden);

    syncAdvancedRow.click();
    flush();

    assertEquals(
        routes.SYNC_ADVANCED.path, Router.getInstance().getCurrentRoute().path);
  });

  // The sync dashboard is not accessible by supervised
  // users, so it should remain hidden.
  test('SyncDashboardHiddenFromSupervisedUsers', function() {
    const dashboardLink =
        syncPage.shadowRoot!.querySelector<HTMLElement>('#syncDashboardLink')!;

    const prefs = getSyncAllPrefs();
    webUIListenerCallback('sync-prefs-changed', prefs);

    // Normal user
    webUIListenerCallback('sync-status-changed', {
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(dashboardLink.hidden);

    // Supervised user
    webUIListenerCallback('sync-status-changed', {
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertTrue(dashboardLink.hidden);
  });

  // ##################################
  // TESTS THAT ARE SKIPPED ON CHROMEOS
  // ##################################


  // <if expr="not is_chromeos">
  test('SyncSetupCancel', async function() {
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      firstSetupInProgress: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    simulateStoredAccounts([{email: 'foo@foo.com'}]);

    const cancelButton =
        syncPage.shadowRoot!.querySelector('settings-sync-account-control')!
            .shadowRoot!.querySelector<HTMLElement>(
                '#setup-buttons cr-button:not(.action-button)');

    assertTrue(!!cancelButton);

    // Clicking the setup cancel button aborts sync.
    cancelButton.click();
    const abort = await browserProxy.whenCalled('didNavigateAwayFromSyncPage');
    assertTrue(abort);
  });

  test('SyncSetupConfirm', async function() {
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      firstSetupInProgress: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    simulateStoredAccounts([{email: 'foo@foo.com'}]);

    const confirmButton =
        syncPage.shadowRoot!.querySelector('settings-sync-account-control')!
            .shadowRoot!.querySelector<HTMLElement>(
                '#setup-buttons .action-button');

    assertTrue(!!confirmButton);
    confirmButton.click();

    const abort = await browserProxy.whenCalled('didNavigateAwayFromSyncPage');
    assertFalse(abort);
  });

  test('SyncSetupLeavePage', async function() {
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      firstSetupInProgress: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();

    // Navigating away while setup is in progress opens the 'Cancel sync?'
    // dialog.
    const router = Router.getInstance();
    router.navigateTo(routes.BASIC);
    await eventToPromise('cr-dialog-open', syncPage);
    assertEquals(router.getRoutes().SYNC, router.getCurrentRoute());
    assertTrue(syncPage.shadowRoot!
                   .querySelector<CrDialogElement>('#setupCancelDialog')!.open);

    // Clicking the cancel button on the 'Cancel sync?' dialog closes
    // the dialog and removes it from the DOM.
    syncPage.shadowRoot!.querySelector<CrDialogElement>('#setupCancelDialog')!
        .querySelector<HTMLElement>('.cancel-button')!.click();
    await eventToPromise(
        'close',
        syncPage.shadowRoot!.querySelector<CrDialogElement>(
            '#setupCancelDialog')!);
    flush();
    assertEquals(router.getRoutes().SYNC, router.getCurrentRoute());
    assertFalse(!!syncPage.shadowRoot!.querySelector<CrDialogElement>(
        '#setupCancelDialog'));

    // Navigating away while setup is in progress opens the
    // dialog again.
    router.navigateTo(routes.BASIC);
    await eventToPromise('cr-dialog-open', syncPage);
    assertTrue(syncPage.shadowRoot!
                   .querySelector<CrDialogElement>('#setupCancelDialog')!.open);

    // Clicking the confirm button on the dialog aborts sync.
    syncPage.shadowRoot!.querySelector<CrDialogElement>('#setupCancelDialog')!
        .querySelector<HTMLElement>('.action-button')!.click();
    const abort = await browserProxy.whenCalled('didNavigateAwayFromSyncPage');
    assertTrue(abort);
  });

  // Tests that entering existing passhrase doesn't abort the sync setup.
  // Regression test for https://crbug.com/1279483.
  test('SyncSetupEnterExistingCorrectPassphrase', async function() {
    // Simulate sync setup in progress.
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      firstSetupInProgress: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    simulateStoredAccounts([{email: 'foo@foo.com'}]);

    // Simulate passphrase enabled.
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    // Enter and submit an existing passphrase.
    const existingPassphraseInput =
        syncPage.shadowRoot!.querySelector<CrInputElement>(
            '#existingPassphraseInput');
    assertTrue(!!existingPassphraseInput);
    existingPassphraseInput.value = 'right';
    browserProxy.decryptionPassphraseSuccess = true;
    const submitExistingPassphrase =
        syncPage.shadowRoot!.querySelector<CrButtonElement>(
            '#submitExistingPassphrase');
    await existingPassphraseInput.updateComplete;
    submitExistingPassphrase!.click();

    await browserProxy.whenCalled('setDecryptionPassphrase');
    // The sync setup cancel dialog would appear on next render if the sync
    // setup was stopped.
    await waitBeforeNextRender(syncPage);

    // Entering passphrase should not display the cancel dialog and should not
    // abort the sync setup.
    const router = Router.getInstance();
    assertEquals(router.getRoutes().SYNC, router.getCurrentRoute());
    const setupCancelDialog =
        syncPage.shadowRoot!.querySelector<CrDialogElement>(
            '#setupCancelDialog');
    assertFalse(!!setupCancelDialog);
  });

  // Tests that creating a new passhrase doesn't abort the sync setup.
  // Regression test for https://crbug.com/1279483.
  test('SyncSetupCreatingValidPassphrase', async function() {
    // Simulate sync setup in progress.
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      firstSetupInProgress: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    simulateStoredAccounts([{email: 'foo@foo.com'}]);

    // Create and submit a new passphrase.
    encryptWithPassphrase.click();
    await eventToPromise('selected-changed', encryptionRadioGroup);
    const passphraseInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseInput')!;
    const passphraseConfirmationInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseConfirmationInput')!;
    passphraseInput.value = 'foo';
    passphraseConfirmationInput.value = 'foo';
    browserProxy.encryptionPassphraseSuccess = true;
    const saveNewPassphrase =
        encryptionElement.shadowRoot!.querySelector<CrButtonElement>(
            '#saveNewPassphrase');
    await Promise.all([
      passphraseInput.updateComplete,
      passphraseConfirmationInput.updateComplete,
    ]);
    saveNewPassphrase!.click();

    await browserProxy.whenCalled('setEncryptionPassphrase');
    // The sync setup cancel dialog would appear on next render if the sync
    // setup was stopped.
    await waitBeforeNextRender(syncPage);

    // Creating passphrase should not display the cancel dialog and should not
    // abort the sync setup.
    const router = Router.getInstance();
    assertEquals(router.getRoutes().SYNC, router.getCurrentRoute());
    const setupCancelDialog =
        syncPage.shadowRoot!.querySelector<CrDialogElement>(
            '#setupCancelDialog');
    assertFalse(!!setupCancelDialog);
  });

  test('SyncSetupSearchSettings', async function() {
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      firstSetupInProgress: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();

    // Searching settings while setup is in progress cancels sync.
    const router = Router.getInstance();
    router.navigateTo(
        router.getRoutes().BASIC, new URLSearchParams('search=foo'));

    const abort = await browserProxy.whenCalled('didNavigateAwayFromSyncPage');
    assertTrue(abort);
  });

  test('ShowAccountRow', function() {
    assertFalse(
        !!syncPage.shadowRoot!.querySelector('settings-sync-account-control'));
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: false,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(
        !!syncPage.shadowRoot!.querySelector('settings-sync-account-control'));
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertTrue(
        !!syncPage.shadowRoot!.querySelector('settings-sync-account-control'));
  });

  test('ShowAccountRow_SigninAllowedFalse', function() {
    loadTimeData.overrideValues({signinAllowed: false});
    setupSyncPage();

    assertFalse(
        !!syncPage.shadowRoot!.querySelector('settings-sync-account-control'));
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(
        !!syncPage.shadowRoot!.querySelector('settings-sync-account-control'));
    webUIListenerCallback('sync-status-changed', {
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(
        !!syncPage.shadowRoot!.querySelector('settings-sync-account-control'));
  });
  // </if>
});

// <if expr="not is_chromeos">
suite('SyncSettingsWithReplaceSyncPromosWithSignInPromos', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
    });
    resetRouterForTesting();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);

    Router.getInstance().navigateTo(routes.SYNC);

    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
  });

  test('DontShowPageWhenReplacingWithSigninPromoAndNotSyncing', function() {
    assertEquals(routes.PEOPLE, Router.getInstance().getCurrentRoute());
  });
});
// </if>

suite('EEAChoiceCountry', function() {
  let syncPage: SettingsSyncPageElement;
  let openWindowProxy: TestOpenWindowProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      signinAllowed: true,
      isEeaChoiceCountry: true,
    });
  });

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);

    // Start with Sync All with no encryption selected. Also, ensure
    // that this is not a supervised user, so that Sync Passphrase is
    // enabled.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    syncPage.set('syncStatus', {
      signedInState: SignedInState.SYNCING,
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
  });

  teardown(function() {
    syncPage.remove();
  });

  test('personalizationControlsVisibility', function() {
    assertFalse(isChildVisible(syncPage, '#activityControlsLinkRowV2'));
    assertTrue(isChildVisible(syncPage, '#personalizationExpandButton'));
  });

  test('personalizationCollapse', async function() {
    // The collapse is collapsed by default.
    const personalizationCollapse =
        syncPage.shadowRoot!.querySelector<CrCollapseElement>(
            '#personalizationCollapse');
    assertTrue(!!personalizationCollapse);
    assertFalse(personalizationCollapse.opened);

    // Clicking the expand-button expands the collapse.
    const expandButton = syncPage.shadowRoot!.querySelector<HTMLElement>(
        '#personalizationExpandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await flushTasks();
    assertTrue(personalizationCollapse.opened);

    // Clicking the expand-button again collapses the collapse.
    expandButton.click();
    await flushTasks();
    assertFalse(personalizationCollapse.opened);
  });

  test('linkedServicesClick', async function() {
    // The linkedServices row is only visible when the collapse is expanded.
    const expandButton = syncPage.shadowRoot!.querySelector<HTMLElement>(
        '#personalizationExpandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await flushTasks();

    const linkedServicesLinkRow =
        syncPage.shadowRoot!.querySelector<HTMLElement>(
            '#linkedServicesLinkRow');
    assertTrue(!!linkedServicesLinkRow);
    linkedServicesLinkRow.click();
    assertEquals(
        'Sync_OpenLinkedServicesPage',
        await metricsBrowserProxy.whenCalled('recordAction'));
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('linkedServicesUrl'), url);
  });
});

