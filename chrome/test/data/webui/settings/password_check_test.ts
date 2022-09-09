// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Check Password tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordCheckListItemElement, SettingsPasswordCheckElement, SettingsPasswordRemoveConfirmationDialogElement} from 'chrome://settings/lazy_load.js';
import {OpenWindowProxyImpl, PasswordCheckInteraction, PasswordManagerImpl, Router, routes, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
// <if expr="chromeos_ash">
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// </if>
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {makeCompromisedCredential, makeInsecureCredential, makePasswordCheckStatus} from './passwords_and_autofill_fake_data.js';
import {getSyncAllPrefs, simulateSyncStatus} from './sync_test_util.js';
import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

function createCheckPasswordSection(): SettingsPasswordCheckElement {
  // Create a passwords-section to use for testing.
  const passwordsSection = document.createElement('settings-password-check');
  document.body.appendChild(passwordsSection);
  flush();
  return passwordsSection;
}

/**
 * Helper method used to create a compromised list item.
 */
function createLeakedPasswordItem(
    entry: chrome.passwordsPrivate.PasswordUiEntry):
    PasswordCheckListItemElement {
  const leakedPasswordItem = document.createElement('password-check-list-item');
  leakedPasswordItem.item = entry;
  document.body.appendChild(leakedPasswordItem);
  flush();
  return leakedPasswordItem;
}

function isElementVisible(elementOrRoot: HTMLElement|ShadowRoot) {
  function composedOffsetParent(node: HTMLElement|ShadowRoot) {
    let offsetParent = (node as HTMLElement).offsetParent;
    let ancestor = node;
    let foundInsideSlot = false;
    while (ancestor && ancestor !== offsetParent) {
      const assignedSlot = (ancestor as HTMLElement).assignedSlot;
      if (assignedSlot) {
        let newOffsetParent = assignedSlot.offsetParent;

        if (getComputedStyle(assignedSlot)['display'] === 'contents') {
          const hadStyleAttribute = assignedSlot.hasAttribute('style');
          const oldDisplay = assignedSlot.style.display;
          assignedSlot.style.display =
              getComputedStyle(ancestor as HTMLElement).display;

          newOffsetParent = assignedSlot.offsetParent;

          assignedSlot.style.display = oldDisplay;
          if (!hadStyleAttribute) {
            assignedSlot.removeAttribute('style');
          }
        }

        ancestor = assignedSlot as HTMLElement | ShadowRoot;
        if (offsetParent !== newOffsetParent) {
          offsetParent = newOffsetParent;
          foundInsideSlot = true;
        }
      } else if ((ancestor as ShadowRoot).host && foundInsideSlot) {
        break;
      }
      ancestor = ((ancestor as ShadowRoot).host || ancestor.parentNode) as
              HTMLElement |
          ShadowRoot;
    }
    return offsetParent;
  }

  const element = elementOrRoot as HTMLElement;
  return !!element && !element.hidden && element.style.display !== 'none' &&
      composedOffsetParent(elementOrRoot) !==
      null;  // Considers parents hiding |element|.
}

/**
 * Helper method used to create a remove password confirmation dialog.
 */
function createRemovePasswordDialog(
    entry: chrome.passwordsPrivate.PasswordUiEntry):
    SettingsPasswordRemoveConfirmationDialogElement {
  const element =
      document.createElement('settings-password-remove-confirmation-dialog');
  element.item = entry;
  document.body.appendChild(element);
  flush();
  return element;
}

/**
 * Helper method used to randomize array.
 */
function shuffleArray<T>(array: T[]): T[] {
  const copy = array.slice();
  for (let i = copy.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    const temp = copy[i]!;
    copy[i] = copy[j]!;
    copy[j] = temp!;
  }
  return copy;
}

/**
 * Helper method to convert |CompromiseType| to string.
 */
function getCompromiseType(
    compromiseType: chrome.passwordsPrivate.CompromiseType): string {
  switch (compromiseType) {
    case chrome.passwordsPrivate.CompromiseType.PHISHED:
      return loadTimeData.getString('phishedPassword');
    case chrome.passwordsPrivate.CompromiseType.LEAKED:
      return loadTimeData.getString('leakedPassword');
    case chrome.passwordsPrivate.CompromiseType.PHISHED_AND_LEAKED:
      return loadTimeData.getString('phishedAndLeakedPassword');
    default:
      assertNotReached();
  }
}

/**
 * Helper method that returns the element that contains the insecure credentials
 * list. If both flags are false, weak credentials list will be returned.
 */
function getElementsByType(
    section: SettingsPasswordCheckElement, isCompromised: boolean,
    isMuted: boolean): HTMLElement {
  if (isCompromised && isMuted) {
    return section.$.mutedPasswordList;
  }
  if (isCompromised) {
    return section.$.leakedPasswordList;
  }
  return section.$.weakPasswordList;
}

/**
 * Helper method that validates a that elements in the insecure credentials list
 * match the expected data.
 * @param checkPasswordSection The section element that will be checked.
 * @param passwordList The expected data.
 * @param isCompromised If true, check compromised info for each insecure
 *     credential.
 * @param isMuted If true, look for the muted passwords section.
 */
function validateInsecurePasswordsList(
    checkPasswordSection: SettingsPasswordCheckElement,
    insecureCredentials: chrome.passwordsPrivate.PasswordUiEntry[],
    isCompromised: boolean, isMuted: boolean = false) {
  const listElements =
      getElementsByType(checkPasswordSection, isCompromised, isMuted);
  assertEquals(
      listElements.querySelector('dom-repeat')!.items!.length,
      insecureCredentials.length);
  const nodes = listElements.querySelectorAll('password-check-list-item');
  for (let index = 0; index < insecureCredentials.length; ++index) {
    const node = nodes[index];
    assertTrue(!!node);
    assertEquals(
        node.$.insecureUsername.textContent!.trim(),
        insecureCredentials[index]!.username);
    assertEquals(
        node.$.insecureOrigin.textContent!.trim(),
        insecureCredentials[index]!.urls.shown);

    if (isCompromised && !isMuted) {
      assertEquals(
          node.shadowRoot!.querySelector('#elapsedTime')!.textContent!.trim(),
          insecureCredentials[index]!.compromisedInfo!
              .elapsedTimeSinceCompromise);
      assertEquals(
          node.shadowRoot!.querySelector('#leakType')!.textContent!.trim(),
          getCompromiseType(
              insecureCredentials[index]!.compromisedInfo!.compromiseType));
    }
  }
}

/**
 * Helper method that validates a that elements in the compromised credentials
 * list match the expected data.
 * @param checkPasswordSection The section element that will be checked.
 * @param passwordList The expected data.
 * @param isMuted If true, look for the muted passwords section.
 */
function validateLeakedPasswordsList(
    checkPasswordSection: SettingsPasswordCheckElement,
    compromisedCredentials: chrome.passwordsPrivate.PasswordUiEntry[],
    isMuted = false) {
  validateInsecurePasswordsList(
      checkPasswordSection, compromisedCredentials, /*isCompromised*/ true,
      isMuted);
}

suite('PasswordsCheckSection', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;
  let passwordManager: TestPasswordManagerProxy;

  let syncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    // Override the SyncBrowserProxyImpl for testing.
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    syncBrowserProxy.testSyncStatus = {
      signedIn: false,
      signedInUsername: '',
      statusAction: StatusAction.NO_ACTION,
    };
  });

  // Test verifies that clicking 'Check again' make proper function call to
  // password manager
  test('checkAgainButtonWhenIdleAfterFirstRun', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.IDLE,
        /*checked=*/ undefined,
        /*remaining=*/ undefined,
        /*lastCheck=*/ 'Just now');
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswordsAgain'),
        section.$.controlPasswordCheckButton.innerText);
    section.$.controlPasswordCheckButton.click();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.START_CHECK_MANUALLY, interaction);
  });

  // Test verifies that clicking 'Start Check' make proper function call to
  // password manager
  test('startCheckButtonWhenIdle', async function() {
    assertEquals(
        PasswordCheckState.IDLE, passwordManager.data.checkStatus.state);
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswords'),
        section.$.controlPasswordCheckButton.innerText);
    section.$.controlPasswordCheckButton.click();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.START_CHECK_MANUALLY, interaction);
  });

  // Test verifies that clicking 'Check again' make proper function call to
  // password manager
  test('stopButtonWhenRunning', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING,
        /*checked=*/ 0,
        /*remaining=*/ 2);
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswordsStop'),
        section.$.controlPasswordCheckButton.innerText);
    section.$.controlPasswordCheckButton.click();
    await passwordManager.whenCalled('stopBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.STOP_CHECK, interaction);
  });

  // Test verifies that sync users see only the link to account checkup and no
  // button to start the local leak check once they run out of quota.
  test('onlyCheckupLinkAfterHittingQuotaWhenSyncing', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.QUOTA_LIMIT);

    const section = createCheckPasswordSection();
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    await passwordManager.whenCalled('getPasswordCheckStatus');
    simulateSyncStatus({
      signedIn: true,
      signedInUsername: 'username',
      statusAction: StatusAction.NO_ACTION,
    });
    assertEquals(
        section.i18n('checkPasswordsErrorQuotaGoogleAccount'),
        section.$.title.innerText);
    assertFalse(isElementVisible(section.$.controlPasswordCheckButton));
  });

  // Test verifies that non-sync users see neither the link to the account
  // checkup nor a retry button once they run out of quota.
  test('noCheckupLinkAfterHittingQuotaWhenSignedOut', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.QUOTA_LIMIT);

    const section = createCheckPasswordSection();
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertEquals(
        section.i18n('checkPasswordsErrorQuota'), section.$.title.innerText);
    assertFalse(isElementVisible(section.$.controlPasswordCheckButton));
  });

  // Test verifies that custom passphrase users see neither the link to the
  // account checkup nor a retry button once they run out of quota.
  test('noCheckupLinkAfterHittingQuotaForEncryption', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.QUOTA_LIMIT);

    const section = createCheckPasswordSection();
    const syncPrefs = getSyncAllPrefs();
    syncPrefs.encryptAllData = true;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    await passwordManager.whenCalled('getPasswordCheckStatus');
    simulateSyncStatus({
      signedIn: true,
      signedInUsername: 'username',
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertEquals(
        section.i18n('checkPasswordsErrorQuota'), section.$.title.innerText);
    assertFalse(isElementVisible(section.$.controlPasswordCheckButton));
  });

  // Test verifies that 'Try again' visible and working when users encounter a
  // generic error.
  test('showRetryAfterGenericError', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.OTHER_ERROR);
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswordsAgainAfterError'),
        section.$.controlPasswordCheckButton.innerText);
    section.$.controlPasswordCheckButton.click();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.START_CHECK_MANUALLY, interaction);
  });

  // Test verifies that 'Check again' is shown when user is signed out.
  test('showCheckAgainWhenSignedOut', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.SIGNED_OUT);
    const section = createCheckPasswordSection();
    webUIListenerCallback('stored-accounts-updated', []);
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswordsAgain'),
        section.$.controlPasswordCheckButton.innerText);
    section.$.controlPasswordCheckButton.click();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.START_CHECK_MANUALLY, interaction);
  });

  // Test verifies that 'Try again' is hidden when users encounter a
  // no-saved-passwords error.
  test('hideRetryAfterNoPasswordsError', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.NO_PASSWORDS);
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertFalse(isElementVisible(section.$.controlPasswordCheckButton));
  });

  // Test verifies that 'Try again' visible and working when users encounter a
  // connection error.
  test('showRetryAfterNoConnectionError', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.OFFLINE);
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswordsAgainAfterError'),
        section.$.controlPasswordCheckButton.innerText);
    section.$.controlPasswordCheckButton.click();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.START_CHECK_MANUALLY, interaction);
  });

  // Test verifies that if no compromised credentials found than list of
  // compromised credentials is not shown, if user is signed in.
  test('noCompromisedCredentialsDisableWeakCheck', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.IDLE,
        /*checked=*/ 4,
        /*remaining=*/ 0,
        /*lastCheck=*/ 'Just now');
    data.leakedCredentials = [];

    const section = createCheckPasswordSection();
    assertFalse(isElementVisible(section.$.noCompromisedCredentials));
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());

    // Initialize with dummy data breach detection settings
    section.prefs = {profile: {password_manager_leak_detection: {value: true}}};

    await passwordManager.whenCalled('getPasswordCheckStatus');
    simulateSyncStatus({
      signedIn: true,
      signedInUsername: 'username',
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(isElementVisible(section.$.compromisedCredentialsBody));
    assertTrue(isElementVisible(section.$.noCompromisedCredentials));
  });

  // Test verifies that compromised credentials are displayed in a proper way.
  test('someCompromisedCredentials', async function() {
    const leakedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.PHISHED, /*id*/ 1,
          /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'two.com', /*username*/ 'test3',
          /*type*/ CompromiseType.LEAKED, /*id*/ 2,
          /*elapsedMinSinceCompromise*/ 2),
    ];
    passwordManager.data.leakedCredentials = leakedPasswords;
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();

    assertFalse(checkPasswordSection.$.compromisedCredentialsBody.hidden);
    assertTrue(checkPasswordSection.$.noCompromisedCredentials.hidden);
    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#expandMutedLeakedCredentials'));
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
    validateLeakedPasswordsList(checkPasswordSection, [], true);
  });

  // Test verifies that compromised credentials are displayed in a proper way
  // when dismiss compromised passwords option is enabled and dismissed
  // passwords exist.
  test('showMutedPasswordsWhenOptionEnabled', async function() {
    const leakedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.PHISHED, /*id*/ 1,
          /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'two.com', /*username*/ 'test3',
          /*type*/ CompromiseType.LEAKED, /*id*/ 2,
          /*elapsedMinSinceCompromise*/ 2),
    ];
    const mutedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'three.com', /*username*/ 'test2',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 3, /*elapsedMinSinceCompromise*/ 3, /*isMuted*/ true),
      makeCompromisedCredential(
          /*url*/ 'four.com', /*username*/ 'test1',
          /*type*/ CompromiseType.LEAKED, /*id*/ 4,
          /*elapsedMinSinceCompromise*/ 4, /*isMuted*/ true),
    ];
    passwordManager.data.leakedCredentials =
        leakedPasswords.concat(mutedPasswords);
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();
    assertFalse(checkPasswordSection.$.compromisedCredentialsBody.hidden);
    assertTrue(checkPasswordSection.$.noCompromisedCredentials.hidden);
    assertTrue(!!checkPasswordSection.shadowRoot!.querySelector(
        '#expandMutedLeakedCredentialsButton'));
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
    validateLeakedPasswordsList(
        checkPasswordSection, mutedPasswords, /*isMuted*/ true);
  });

  // Test verifies that credentials from mobile app shown correctly.
  test('someCompromisedCredentials', function() {
    const password = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4',
        /*type*/ CompromiseType.LEAKED);
    password.changePasswordUrl = undefined;

    const checkPasswordSection = createLeakedPasswordItem(password);
    assertEquals(
        checkPasswordSection.shadowRoot!.querySelector('changePasswordUrl'),
        null);
    assertTrue(!!checkPasswordSection.shadowRoot!.querySelector(
        '#changePasswordInApp'));
  });

  // Verify that a click on "Change password" opens the expected URL and
  // records a corresponding user action.
  test('changePasswordOpensUrlAndRecordsAction', async function() {
    const testOpenWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(testOpenWindowProxy);

    const password = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4',
        /*type*/ CompromiseType.LEAKED);
    const passwordCheckListItem = createLeakedPasswordItem(password);
    const button = passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
        '#changePasswordButton');
    assertTrue(!!button);
    button.click();

    const url = await testOpenWindowProxy.whenCalled('openURL');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals('http://one.com/', url);
    assertEquals(PasswordCheckInteraction.CHANGE_PASSWORD, interaction);
  });

  // Verify that a click on "Change password" starts an Automatic Password
  // Change flow.
  test(
      'changePasswordOpensAutomaticPasswordChangeAndRecordsAction',
      async function() {
        const password = makeCompromisedCredential(
            /*url*/ 'one.com', /*username*/ 'test4',
            /*type*/ CompromiseType.LEAKED);
        password.hasStartableScript = true;
        const passwordCheckListItem = createLeakedPasswordItem(password);
        const button =
            passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
                '#changePasswordButton');
        assertTrue(!!button);
        button.click();

        const credentialApc =
            await passwordManager.whenCalled('startAutomatedPasswordChange');
        assertEquals(credentialApc, password);
        const interaction =
            await passwordManager.whenCalled('recordPasswordCheckInteraction');
        assertEquals(
            PasswordCheckInteraction.CHANGE_PASSWORD_AUTOMATICALLY,
            interaction);
      });

  // Verify that elements without a startable script have the correct icon.
  test('iconIsCorrectForPasswordWithoutScript', async function() {
    const password = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4',
        /*type*/ CompromiseType.LEAKED);
    password.hasStartableScript = false;
    const passwordCheckListItem = createLeakedPasswordItem(password);

    const manualChangeIcon =
        passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
            '#change-password-link-icon');
    assertTrue(!!manualChangeIcon);

    const automatedChangeIcon =
        passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
            '#change-password-automatically-icon');
    assertFalse(!!automatedChangeIcon);
  });

  // Verify that elements with a startable script have the correct icon.
  test('iconIsCorrectForPasswordWithScript', async function() {
    const password = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4',
        /*type*/ CompromiseType.LEAKED);
    password.hasStartableScript = true;
    const passwordCheckListItem = createLeakedPasswordItem(password);

    const manualChangeIcon =
        passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
            '#change-password-link-icon');
    assertFalse(!!manualChangeIcon);

    const automatedChangeIcon =
        passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
            '#change-password-automatically-icon');
    assertTrue(!!automatedChangeIcon);
  });

  // Verify that for a leaked password the More Actions menu opens when the
  // button is clicked.
  // If dismiss compromised password option is enabled and if the clicked item
  // is a leaked password: Menu must have a dismiss button.
  test('moreActionsMenuWithMuteButton', async function() {
    passwordManager.data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'google.com', /*username*/ 'derinel',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 1, /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ false),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();
    assertFalse(checkPasswordSection.$.compromisedCredentialsBody.hidden);
    const listElement = checkPasswordSection.shadowRoot!.querySelector(
        'password-check-list-item')!;
    const menu = checkPasswordSection.$.moreActionsMenu;

    assertFalse(menu.open);
    listElement.$.more.click();
    flush();
    assertTrue(menu.open);

    assertTrue(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuMuteCompromisedPassword'));
    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuUnmuteMutedCompromisedPassword'));
  });

  // Verify that for a leaked password the More Actions menu opens when the
  // button is clicked.
  // If dismiss compromised password option is enabled but disabled by prefs,
  // and if the clicked item is a leaked password: Menu must have a disabled
  // dismiss button.
  test('moreActionsMenuWithMuteButtonDisabled', async function() {
    passwordManager.data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'google.com', /*username*/ 'derinel',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 1, /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ false),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    checkPasswordSection.prefs = {
      profile: {
        password_dismiss_compromised_alert: {value: false},
        password_manager_leak_detection: {value: true},
      },
    };
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();
    assertFalse(checkPasswordSection.$.compromisedCredentialsBody.hidden);
    const listElement = checkPasswordSection.shadowRoot!.querySelector(
        'password-check-list-item')!;
    const menu = checkPasswordSection.$.moreActionsMenu;

    assertFalse(menu.open);
    listElement.$.more.click();
    flush();
    assertTrue(menu.open);

    assertTrue(checkPasswordSection.shadowRoot!
                   .querySelector<HTMLButtonElement>(
                       '#menuMuteCompromisedPassword')!.disabled);
    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuUnmuteMutedCompromisedPassword'));
  });

  // Verify that for a leaked password the More Actions menu opens when the
  // button is clicked.
  // If dismiss compromised password option is enabled and if the clicked item
  // is a leaked password: Menu must have a dismiss button.
  test('moreActionsMenuWithUnmuteButton', async function() {
    passwordManager.data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'google.com', /*username*/ 'derinel',
          /*type*/ CompromiseType.LEAKED, 1,
          /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ true),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();
    assertFalse(checkPasswordSection.$.compromisedCredentialsBody.hidden);
    const listElement = checkPasswordSection.shadowRoot!.querySelector(
        'password-check-list-item')!;
    const menu = checkPasswordSection.$.moreActionsMenu;

    assertFalse(menu.open);
    listElement.$.more.click();
    flush();
    assertTrue(menu.open);

    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuMuteCompromisedPassword'));
    assertTrue(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuUnmuteMutedCompromisedPassword'));
  });

  // Verify that for a leaked password the More Actions menu opens when the
  // button is clicked.
  // If dismiss compromised password option is enabled but disabled by prefs and
  // if the clicked item is a leaked password: Menu must have a disabled dismiss
  // button.
  test('moreActionsMenuWithUnmuteButtonDisabled', async function() {
    passwordManager.data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'google.com', /*username*/ 'derinel',
          /*type*/ CompromiseType.LEAKED, 1,
          /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ true),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    checkPasswordSection.prefs = {
      profile: {
        password_dismiss_compromised_alert: {value: false},
        password_manager_leak_detection: {value: true},
      },
    };
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();
    assertFalse(checkPasswordSection.$.compromisedCredentialsBody.hidden);
    const listElement = checkPasswordSection.shadowRoot!.querySelector(
        'password-check-list-item')!;
    const menu = checkPasswordSection.$.moreActionsMenu;

    assertFalse(menu.open);
    listElement.$.more.click();
    flush();
    assertTrue(menu.open);

    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuMuteCompromisedPassword'));
    assertTrue(checkPasswordSection.shadowRoot!
                   .querySelector<HTMLButtonElement>(
                       '#menuUnmuteMutedCompromisedPassword')!.disabled);
  });

  // Verify that for a weak password the More Actions menu opens when the
  // button is clicked.
  // If dismiss compromised password option is enabled and if the clicked item
  // is a weak password: Menu should not have mute / dismiss buttons.
  test('moreActionsMenuForWeakPasswords', async function() {
    passwordManager.data.weakCredentials = [
      makeInsecureCredential(/*url*/ 'google.com', /*username*/ 'derinel'),
    ];
    const checkPasswordSection = createCheckPasswordSection();

    await passwordManager.whenCalled('getWeakCredentials');
    flush();
    assertFalse(checkPasswordSection.$.weakCredentialsBody.hidden);
    const listElement = checkPasswordSection.shadowRoot!.querySelector(
        'password-check-list-item')!;
    const menu = checkPasswordSection.$.moreActionsMenu;
    assertFalse(menu.open);
    listElement.$.more.click();
    assertTrue(menu.open);
    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuMuteCompromisedPassword'));
    assertFalse(!!checkPasswordSection.shadowRoot!.querySelector(
        '#menuUnmuteMutedCompromisedPassword'));
  });

  // Test verifies that clicking remove button is calling proper
  // proxy function.
  test('removePasswordConfirmationDialog', async function() {
    const entry = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4', /*type*/ CompromiseType.LEAKED,
        /*id*/ 0);
    const removeDialog = createRemovePasswordDialog(entry);
    removeDialog.$.remove.click();
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    const {id, fromStores} =
        await passwordManager.whenCalled('removeSavedPassword');

    assertEquals(PasswordCheckInteraction.REMOVE_PASSWORD, interaction);
    assertEquals(0, id);
    assertEquals(entry.storedIn, fromStores);
  });

  // Test verifies that clicking dismiss button is calling proper proxy
  // function.
  test('mutePasswordButtonCallsBackend', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'username',
        /*type*/ CompromiseType.LEAKED)];
    const checkPasswordSection = createCheckPasswordSection();

    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();

    const listElements = checkPasswordSection.$.leakedPasswordList;
    const node = listElements.children[0] as PasswordCheckListItemElement;

    // Open the more actions menu and click 'Dismiss password'.
    node.$.more.click();
    flush();
    checkPasswordSection.shadowRoot!
        .querySelector<HTMLElement>('#menuMuteCompromisedPassword')!.click();

    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.MUTE_PASSWORD, interaction);
    assertEquals(1, passwordManager.getCallCount('muteInsecureCredential'));
  });

  // Test verifies that clicking restore button is calling proper proxy
  // function.
  test('unmutePasswordButtonCallsBackend', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'username',
        /*type*/ CompromiseType.LEAKED,
        /*id*/ 1, /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ true)];
    const checkPasswordSection = createCheckPasswordSection();

    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();

    const listElements = checkPasswordSection.$.mutedPasswordList;
    const node = listElements.children[0] as PasswordCheckListItemElement;

    // Open the more actions menu and click 'Dismiss password'.
    node.$.more.click();
    flush();
    checkPasswordSection.shadowRoot!
        .querySelector<HTMLElement>(
            '#menuUnmuteMutedCompromisedPassword')!.click();

    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.UNMUTE_PASSWORD, interaction);
    assertEquals(1, passwordManager.getCallCount('unmuteInsecureCredential'));
  });

  // Tests that a secure change password URL gets linkified in the remove
  // password confirmation dialog.
  test('secureChangePasswordUrlInRemovePasswordConfirmationDialog', () => {
    const entry = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4', /*type*/ CompromiseType.LEAKED,
        /*id*/ 0);
    entry.changePasswordUrl = 'https://one.com';
    const removeDialog = createRemovePasswordDialog(entry);
    assertTrue(isElementVisible(removeDialog.$.link));
    assertFalse(isElementVisible(removeDialog.$.text));
  });

  // Tests that an insecure change password URL does not get linkified in the
  // remove password confirmation dialog.
  test('insecureChangePasswordUrlInRemovePasswordConfirmationDialog', () => {
    const entry = makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4', /*type*/ CompromiseType.LEAKED,
        /*id*/ 0);
    entry.changePasswordUrl = 'http://one.com';
    const removeDialog = createRemovePasswordDialog(entry);
    assertFalse(isElementVisible(removeDialog.$.link));
    assertTrue(isElementVisible(removeDialog.$.text));
  });

  // A changing status is immediately reflected in title, icon and banner.
  test('updatesNumberOfCheckedPasswordsWhileRunning', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING,
        /*checked=*/ 0,
        /*remaining=*/ 2);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertTrue(isElementVisible(section.$.title));
    assertEquals(
        section.i18n('checkPasswordsProgress', 1, 2),
        section.$.title.innerText);

    // Change status from running to IDLE.
    assertTrue(!!passwordManager.lastCallback.addPasswordCheckStatusListener);
    passwordManager.lastCallback.addPasswordCheckStatusListener(
        makePasswordCheckStatus(
            /*state=*/ PasswordCheckState.RUNNING,
            /*checked=*/ 1,
            /*remaining=*/ 1));

    flush();
    assertTrue(isElementVisible(section.$.title));
    assertEquals(
        section.i18n('checkPasswordsProgress', 2, 2),
        section.$.title.innerText);
  });

  // Tests that the status is queried right when the page loads.
  test('queriesCheckedStatusImmediately', async function() {
    const data = passwordManager.data;
    assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
    assertEquals(0, data.leakedCredentials.length);

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertEquals(PasswordCheckState.IDLE, checkPasswordSection.status.state);
  });

  // Tests that the spinner is replaced with a checkmark on successful runs.
  test('showsCheckmarkIconWhenFinishedWithoutLeaks', async function() {
    const data = passwordManager.data;
    assertEquals(0, data.leakedCredentials.length);
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.IDLE,
        /*checked=*/ undefined,
        /*remaining=*/ undefined,
        /*lastCheck=*/ 'Just now');

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const icon = checkPasswordSection.shadowRoot!.querySelector('iron-icon')!;
    const spinner =
        checkPasswordSection.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertFalse(isElementVisible(spinner));
    assertTrue(isElementVisible(icon));
    assertFalse(icon.classList.contains('has-security-issues'));
    assertTrue(icon.classList.contains('no-security-issues'));
  });

  // Tests that the spinner is replaced with a triangle if leaks were found.
  test('showsTriangleIconWhenFinishedWithLeaks', async function() {
    const data = passwordManager.data;
    assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const icon = checkPasswordSection.shadowRoot!.querySelector('iron-icon')!;
    const spinner =
        checkPasswordSection.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertFalse(isElementVisible(spinner));
    assertTrue(isElementVisible(icon));
    assertTrue(icon.classList.contains('has-security-issues'));
    assertFalse(icon.classList.contains('no-security-issues'));
  });

  // Tests that the spinner is replaced with an info icon if only weak passwords
  // were found.
  test('showsInfoIconWhenFinishedWithWeakPasswords', async function() {
    const data = passwordManager.data;
    assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
    data.weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test5'),
    ];

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const icon = checkPasswordSection.shadowRoot!.querySelector('iron-icon')!;
    const spinner =
        checkPasswordSection.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertFalse(isElementVisible(spinner));
    assertTrue(isElementVisible(icon));
    assertFalse(icon.classList.contains('has-security-issues'));
    assertFalse(icon.classList.contains('no-security-issues'));
  });

  // Test that leaked password items have a strong CTA.
  test('showStrongCtaOnLeaks', function() {
    const passwordCheckListItem =
        createLeakedPasswordItem(makeCompromisedCredential(
            /*url*/ 'one.com', /*username*/ 'test6',
            /*type*/ CompromiseType.LEAKED));
    const shadowRoot = passwordCheckListItem.shadowRoot!;
    assertTrue(
        shadowRoot.querySelector('#changePasswordButton')!.classList.contains(
            'action-button'));
    assertFalse(shadowRoot.querySelector('iron-icon')!.classList.contains(
        'icon-weak-cta'));
  });

  // Test that leaked muted password items have a weak CTA when the dismiss
  // compromised passwords option is enabled.
  test('showWeakCtaOnMutedLeaksIfDismissOptionEnabled', function() {
    const passwordCheckListItem =
        createLeakedPasswordItem(makeCompromisedCredential(
            /*url*/ 'one.com', /*username*/ 'test6',
            /*type*/ CompromiseType.LEAKED,
            /*id*/ 1, /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ true));
    const shadowRoot = passwordCheckListItem.shadowRoot!;
    assertFalse(
        shadowRoot.querySelector('#changePasswordButton')!.classList.contains(
            'action-button'));
    assertTrue(shadowRoot.querySelector('iron-icon')!.classList.contains(
        'icon-weak-cta'));
  });

  // Tests that weak password items have a weak CTA.
  test('showWeakCtaOnWeaksPasswords', function() {
    const passwordCheckListItem = createLeakedPasswordItem(
        makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test7'));
    const shadowRoot = passwordCheckListItem.shadowRoot!;
    assertFalse(
        shadowRoot.querySelector('#changePasswordButton')!.classList.contains(
            'action-button'));
    assertTrue(shadowRoot.querySelector('iron-icon')!.classList.contains(
        'icon-weak-cta'));
  });

  // Tests that the spinner is replaced with a warning on errors.
  test('showsInfoIconWhenFinishedWithErrors', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.OFFLINE,
        /*checked=*/ undefined,
        /*remaining=*/ undefined);

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const icon = checkPasswordSection.shadowRoot!.querySelector('iron-icon')!;
    const spinner =
        checkPasswordSection.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertFalse(isElementVisible(spinner));
    assertTrue(isElementVisible(icon));
    assertFalse(icon.classList.contains('has-security-issues'));
    assertFalse(icon.classList.contains('no-security-issues'));
  });

  // Tests that the spinner replaces any icon while the check is running.
  test('showsSpinnerWhileRunning', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING,
        /*checked=*/ 1,
        /*remaining=*/ 3);

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const icon = checkPasswordSection.shadowRoot!.querySelector('iron-icon')!;
    const spinner =
        checkPasswordSection.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertTrue(isElementVisible(spinner));
    assertFalse(isElementVisible(icon));
  });

  // While running, the check should show the processed and total passwords.
  test('showOnlyProgressWhileRunningWithoutLeaks', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING,
        /*checked=*/ 0,
        /*remaining=*/ 4);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkPasswordsProgress', 1, 4), title.innerText);
    assertFalse(isElementVisible(section.$.subtitle));
  });

  // Verifies that in case the backend could not obtain the number of checked
  // and remaining credentials the UI does not surface 0s to the user.
  test('runningProgressHandlesZeroCaseFromBackend', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING,
        /*checked=*/ 0,
        /*remaining=*/ 0);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkPasswordsProgress', 1, 1), title.innerText);
    assertFalse(isElementVisible(section.$.subtitle));
  });

  // While running, show progress and already found leak count.
  test('showProgressAndLeaksWhileRunning', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING,
        /*checked=*/ 1,
        /*remaining=*/ 4);
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(title));
    assertTrue(isElementVisible(subtitle));
    assertEquals(section.i18n('checkPasswordsProgress', 2, 5), title.innerText);
  });

  // Shows count of insecure credentials, if compromised credentials exist.
  test('showInsecurePasswordsCount', async function() {
    const data = passwordManager.data;
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];
    data.weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test4'),
    ];

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(subtitle));

    const count =
        await PluralStringProxyImpl.getInstance().getPluralStringTupleWithComma(
            'safetyCheckPasswordsCompromised', 1, 'safetyCheckPasswordsWeak',
            1);
    assertEquals(count, subtitle.textContent!.trim());
  });

  // Does not show the count of insecure credentials nor dismissed credentials
  // when the dismiss compromised passwords option is enabled. Shows only the
  // weak password count.
  test('doNotShowInsecurePasswordCount', async function() {
    const data = passwordManager.data;
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED, /*id*/ 1,
          /*elapsedMinSinceCompromise*/ 1, /*isMuted*/ true),
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test5',
          /*type*/ CompromiseType.LEAKED, /*id*/ 2,
          /*elapsedMinSinceCompromise*/ 2, /*isMuted*/ true),
    ];
    data.weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test4'),
    ];

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(subtitle));

    const count = await PluralStringProxyImpl.getInstance().getPluralString(
        'weakPasswords', 1);
    assertEquals(count, subtitle.textContent!.trim());
  });

  // Shows count of weak credentials, if no compromised credentials exist.
  test('showWeakPasswordsCountSignedIn', async function() {
    passwordManager.data.weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test4'),
      makeInsecureCredential(/*url*/ 'two.com', /*username*/ 'test5'),
    ];

    const section = createCheckPasswordSection();
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    await passwordManager.whenCalled('getPasswordCheckStatus');
    simulateSyncStatus({
      signedIn: true,
      signedInUsername: 'username',
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(subtitle));

    const count = await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyCheckPasswordsWeak', 2);
    assertEquals(count, subtitle.textContent!.trim());
  });

  // Shows count of weak credentials, if no compromised credentials exist and
  // the user is signed out.
  test('showWeakPasswordsCountSignedOut', async function() {
    passwordManager.data.weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test4'),
      makeInsecureCredential(/*url*/ 'two.com', /*username*/ 'test5'),
    ];

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(subtitle));

    const count = await PluralStringProxyImpl.getInstance().getPluralString(
        'weakPasswords', 2);
    assertEquals(count, subtitle.textContent!.trim());
  });

  // Verify that weak passwords section is shown.
  test('showWeakPasswordsSyncing', async function() {
    const weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test1'),
      makeInsecureCredential(/*url*/ 'two.com', /*username*/ 'test2'),
    ];
    passwordManager.data.weakCredentials = weakCredentials;

    const section = createCheckPasswordSection();
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    await passwordManager.whenCalled('getPasswordCheckStatus');
    simulateSyncStatus({
      signedIn: true,
      signedInUsername: 'username',
      statusAction: StatusAction.NO_ACTION,
    });
    flush();

    assertTrue(isElementVisible(section.$.weakCredentialsBody));
    assertEquals(
        section.i18n('weakPasswordsDescriptionGeneration'),
        section.$.weakPasswordsDescription.innerText);
    validateInsecurePasswordsList(section, weakCredentials, false);
  });

  test('showWeakPasswordsSignedOut', async function() {
    const weakCredentials = [
      makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test1'),
      makeInsecureCredential(/*url*/ 'two.com', /*username*/ 'test2'),
    ];
    passwordManager.data.weakCredentials = weakCredentials;

    const section = createCheckPasswordSection();
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();

    assertTrue(isElementVisible(section.$.weakCredentialsBody));
    assertEquals(
        section.i18n('weakPasswordsDescription'),
        section.$.weakPasswordsDescription.innerText);
    validateInsecurePasswordsList(section, weakCredentials, false);
  });

  // Verify that weak passwords section is hidden, if no weak credentials were
  // found.
  test('noWeakPasswords', async function() {
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();

    assertFalse(isElementVisible(section.$.weakCredentialsBody));
  });

  // When canceled, show string explaining that and already found leak
  // count.
  test('showProgressAndLeaksAfterCanceled', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.CANCELED,
        /*checked=*/ 2,
        /*remaining=*/ 3);
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(title));
    assertTrue(isElementVisible(subtitle));
    assertEquals(section.i18n('checkPasswordsCanceled'), title.innerText);
  });

  // Before the first run, show nothing.
  test('showOnlyDescriptionIfNotRun', async function() {
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(title));
    assertFalse(isElementVisible(subtitle));
    assertEquals('', title.innerText);
  });

  // After running, show confirmation, timestamp and number of leaks.
  test('showLeakCountAndTimeStampWhenIdle', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.IDLE,
        /*checked=*/ 4,
        /*remaining=*/ 0,
        /*lastCheck=*/ 'Just now');
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const titleRow = section.$.titleRow;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(titleRow));
    assertTrue(isElementVisible(subtitle));
    assertEquals(
        section.i18n('checkedPasswords') + ' • Just now', titleRow.innerText);
  });

  // When offline, only show an error.
  test('showOnlyErrorWhenOffline', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.OFFLINE);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkPasswordsErrorOffline'), title.innerText);
    assertFalse(isElementVisible(section.$.subtitle));
  });

  // When the user is signed out but has run a weak check a timestamp should be
  // shown.
  test('showWeakCheckTimestampWhenSignedOut', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.SIGNED_OUT,
        /*checked=*/ 0,
        /*remaining=*/ 0,
        /*lastCheck=*/ 'Just now');

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const titleRow = section.$.titleRow;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(titleRow));
    assertTrue(isElementVisible(subtitle));
    assertEquals(
        section.i18n('checkedPasswords') + ' • Just now', titleRow.innerText);
  });

  // If user is signed out and has compromised credentials that were found in
  // the past, shows "Checked passwords" and correct label in the top of
  // comromised passwords section.
  test('signedOutHasCompromisedHasWeak', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.SIGNED_OUT);
    passwordManager.data.weakCredentials =
        [makeInsecureCredential(/*url*/ 'one.com', /*username*/ 'test1')];
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'one.com', /*username*/ 'test4', /*type*/ CompromiseType.LEAKED,
        /*id*/ 1)];
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();

    const title = section.$.title;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkedPasswords'), title.innerText);
    assertTrue(isElementVisible(subtitle));
    const count =
        await PluralStringProxyImpl.getInstance().getPluralStringTupleWithComma(
            'safetyCheckPasswordsCompromised', 1, 'safetyCheckPasswordsWeak',
            1);
    assertEquals(count, subtitle.textContent!.trim());

    assertTrue(
        section.shadowRoot!.querySelector('iron-icon')!.classList.contains(
            'has-security-issues'));
    assertFalse(
        section.shadowRoot!.querySelector('iron-icon')!.classList.contains(
            'no-security-issues'));

    assertTrue(isElementVisible(section.$.compromisedCredentialsBody));
    assertTrue(isElementVisible(section.$.signedOutUserLabel));
    assertEquals(
        section.i18n('signedOutUserHasCompromisedCredentialsLabel'),
        section.$.signedOutUserLabel.textContent!.trim());
    assertTrue(isElementVisible(section.$.compromisedPasswordsDescription));
    assertEquals(
        section.i18n('compromisedPasswordsDescription'),
        section.$.compromisedPasswordsDescription.textContent!.trim());
    assertTrue(isElementVisible(section.$.weakCredentialsBody));
  });

  // If user is signed out and doesn't have compromised credentials in the past
  // and doesn't have weak credentials, shows "Checked passwords" and correct
  // label in the top of comromised passwords section.
  test('signedOutNoCompromisedNoWeak', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.SIGNED_OUT,
        /*checked=*/ 0,
        /*remaining=*/ 0,
        /*lastCheck=*/ 'Just now');
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();

    const title = section.$.title;
    const subtitle = section.$.subtitle;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkedPasswords'), title.innerText);
    assertTrue(isElementVisible(subtitle));
    assertTrue(
        section.shadowRoot!.querySelector('iron-icon')!.classList.contains(
            'no-security-issues'));
    assertFalse(
        section.shadowRoot!.querySelector('iron-icon')!.classList.contains(
            'has-security-issues'));

    assertTrue(isElementVisible(section.$.compromisedCredentialsBody));
    assertTrue(isElementVisible(section.$.signedOutUserLabel));
    assertEquals(
        section.i18n('signedOutUserLabel'),
        section.$.signedOutUserLabel.textContent!.trim());
    assertFalse(isElementVisible(section.$.compromisedPasswordsDescription));
  });

  // When no passwords are saved, only show an error.
  test('showOnlyErrorWithoutPasswords', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.NO_PASSWORDS);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    assertTrue(isElementVisible(title));
    assertEquals(
        section.i18n('checkPasswordsErrorNoPasswords'), title.innerText);
    assertFalse(isElementVisible(section.$.subtitle));
  });

  // When users run out of quota, only show an error.
  test('showOnlyErrorWhenQuotaIsHit', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.QUOTA_LIMIT);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkPasswordsErrorQuota'), title.innerText);
    assertFalse(isElementVisible(section.$.subtitle));
  });

  // When a general error occurs, only show the message.
  test('showOnlyGenericError', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.OTHER_ERROR);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const title = section.$.title;
    assertTrue(isElementVisible(title));
    assertEquals(section.i18n('checkPasswordsErrorGeneric'), title.innerText);
    assertFalse(isElementVisible(section.$.subtitle));
  });

  // Transform check-button to stop-button if a check is running.
  test('buttonChangesTextAccordingToStatus', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.IDLE);

    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswords'),
        section.$.controlPasswordCheckButton.innerText);

    // Change status from running to IDLE.
    assertTrue(!!passwordManager.lastCallback.addPasswordCheckStatusListener);
    passwordManager.lastCallback.addPasswordCheckStatusListener(
        makePasswordCheckStatus(
            /*state=*/ PasswordCheckState.RUNNING,
            /*checked=*/ 0,
            /*remaining=*/ 2));
    assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
    assertEquals(
        section.i18n('checkPasswordsStop'),
        section.$.controlPasswordCheckButton.innerText);
  });

  // Test that the banner is in a state that shows the positive confirmation
  // after a leak check finished.
  test('showsPositiveBannerWhenIdle', async function() {
    const data = passwordManager.data;
    assertEquals(0, data.leakedCredentials.length);
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.IDLE,
        /*checked=*/ undefined,
        /*remaining=*/ undefined,
        /*lastCheck=*/ 'Just now');

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const bannerImage =
        checkPasswordSection.shadowRoot!.querySelector<HTMLImageElement>(
            '#bannerImage')!;
    assertTrue(isElementVisible(bannerImage));
    assertEquals(
        'chrome://settings/images/password_check_positive.svg',
        bannerImage.src);
  });

  // Test that the banner indicates a neutral state if no check was run yet.
  test('showsNeutralBannerBeforeFirstRun', async function() {
    const data = passwordManager.data;
    assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
    assertEquals(0, data.leakedCredentials.length);

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const bannerImage =
        checkPasswordSection.shadowRoot!.querySelector<HTMLImageElement>(
            '#bannerImage')!;
    assertTrue(isElementVisible(bannerImage));
    assertEquals(
        'chrome://settings/images/password_check_neutral.svg', bannerImage.src);
  });

  // Test that the banner is in a state that shows that the leak check is
  // in progress but hasn't found anything yet.
  test('showsNeutralBannerWhenRunning', async function() {
    const data = passwordManager.data;
    assertEquals(0, data.leakedCredentials.length);
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING, /*checked=*/ 1,
        /*remaining=*/ 5);

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const bannerImage =
        checkPasswordSection.shadowRoot!.querySelector<HTMLImageElement>(
            '#bannerImage')!;
    assertTrue(isElementVisible(bannerImage));
    assertEquals(
        'chrome://settings/images/password_check_neutral.svg', bannerImage.src);
  });

  // Test that the banner is in a state that shows that the leak check is
  // in progress but hasn't found anything yet.
  test('showsNeutralBannerWhenCanceled', async function() {
    const data = passwordManager.data;
    assertEquals(0, data.leakedCredentials.length);
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.CANCELED);

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    const bannerImage =
        checkPasswordSection.shadowRoot!.querySelector<HTMLImageElement>(
            '#bannerImage')!;
    assertTrue(isElementVisible(bannerImage));
    assertEquals(
        'chrome://settings/images/password_check_neutral.svg', bannerImage.src);
  });

  // Test that the banner isn't visible as soon as the first leak is detected.
  test('leaksHideBannerWhenRunning', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.RUNNING, /*checked=*/ 1,
        /*remaining=*/ 5);
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertFalse(isElementVisible(
        checkPasswordSection.shadowRoot!.querySelector<HTMLElement>(
            '#bannerImage')!));
  });

  // Test that the banner isn't visible if a leak is detected after a check.
  test('leaksHideBannerWhenIdle', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.IDLE);
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertFalse(isElementVisible(
        checkPasswordSection.shadowRoot!.querySelector<HTMLElement>(
            '#bannerImage')!));
  });

  // Test that the banner isn't visible if a leak is detected after canceling.
  test('leaksHideBannerWhenCanceled', async function() {
    const data = passwordManager.data;
    data.checkStatus = makePasswordCheckStatus(
        /*state=*/ PasswordCheckState.CANCELED);
    data.leakedCredentials = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED),
    ];

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getPasswordCheckStatus');
    flush();
    assertFalse(isElementVisible(
        checkPasswordSection.shadowRoot!.querySelector<HTMLElement>(
            '#bannerImage')!));
  });

  // Test verifies that new credentials are added to the bottom.
  test('appendCompromisedCredentials', function() {
    const leakedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED, /*id*/ 1,
          /*elapsedMinSinceCompromise*/ 0),
      makeCompromisedCredential(
          /*url*/ 'two.com', /*username*/ 'test3',
          /*type*/ CompromiseType.LEAKED, /*id*/ 2,
          /*elapsedMinSinceCompromise*/ 0),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    // checkPasswordSection.updateCompromisedPasswordList(leakedPasswords);
    checkPasswordSection.updateCompromisedPasswordList(leakedPasswords);
    flush();

    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);

    leakedPasswords.push(makeCompromisedCredential(
        /*url*/ 'three.com', /*username*/ 'test2',
        /*type*/ CompromiseType.PHISHED, /*id*/ 3,
        /*elapsedMinSinceCompromise*/ 6));
    leakedPasswords.push(makeCompromisedCredential(
        /*url*/ 'four.com', /*username*/ 'test1',
        /*type*/ CompromiseType.LEAKED, /*id*/ 4,
        /*elapsedMinSinceCompromise*/ 4));
    leakedPasswords.push(makeCompromisedCredential(
        /*url*/ 'five.com', /*username*/ 'test0',
        /*type*/ CompromiseType.LEAKED, /*id*/ 5,
        /*elapsedMinSinceCompromise*/ 5));
    checkPasswordSection.updateCompromisedPasswordList(
        shuffleArray(leakedPasswords));
    flush();
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
  });

  // Test verifies that deleting and adding works as it should.
  test('deleteCompromisedCredemtials', function() {
    const leakedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test4',
          /*type*/ CompromiseType.PHISHED, /*id*/ 0,
          /*elapsedMinSinceCompromise*/ 0),
      makeCompromisedCredential(
          /*url*/ '2two.com', /*username*/ 'test3',
          /*type*/ CompromiseType.LEAKED, /*id*/ 1,
          /*elapsedMinSinceCompromise*/ 2),
      makeCompromisedCredential(
          /*url*/ '3three.com', /*username*/ 'test2',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 2, /*elapsedMinSinceCompromise*/ 2),
      makeCompromisedCredential(
          /*url*/ '4four.com', /*username*/ 'test2',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 3, /*elapsedMinSinceCompromise*/ 2),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    checkPasswordSection.updateCompromisedPasswordList(leakedPasswords);
    flush();
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);

    // remove 2nd and 3rd elements
    leakedPasswords.splice(1, 2);
    leakedPasswords.push(makeCompromisedCredential(
        /*url*/ 'five.com', /*username*/ 'test2',
        /*type*/ CompromiseType.LEAKED, /*id*/ 4,
        /*elapsedMinSinceCompromise*/ 5));

    checkPasswordSection.updateCompromisedPasswordList(
        shuffleArray(leakedPasswords));
    flush();
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
  });

  // Test verifies sorting. Phished passwords always shown above leaked even
  // if they are older.
  test('sortCompromisedCredentials', function() {
    const leakedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'one.com', /*username*/ 'test6',
          /*type*/ CompromiseType.PHISHED, /*id*/ 6,
          /*elapsedMinSinceCompromise*/ 3),
      makeCompromisedCredential(
          /*url*/ 'two.com', /*username*/ 'test5',
          /*type*/ CompromiseType.PHISHED_AND_LEAKED, /*id*/ 5,
          /*elapsedMinSinceCompromise*/ 4),
      makeCompromisedCredential(
          /*url*/ 'three.com', /*username*/ 'test4',
          /*type*/ CompromiseType.PHISHED,
          /*id*/ 4, /*elapsedMinSinceCompromise*/ 5),
      makeCompromisedCredential(
          /*url*/ 'four.com', /*username*/ 'test3',
          /*type*/ CompromiseType.LEAKED, /*id*/ 3,
          /*elapsedMinSinceCompromise*/ 0),
      makeCompromisedCredential(
          /*url*/ 'five.com', /*username*/ 'test2',
          /*type*/ CompromiseType.LEAKED, /*id*/ 2,
          /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'six.com', /*username*/ 'test1',
          /*type*/ CompromiseType.LEAKED, /*id*/ 1,
          /*elapsedMinSinceCompromise*/ 2),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    checkPasswordSection.updateCompromisedPasswordList(
        shuffleArray(leakedPasswords));
    flush();
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
  });

  // Test verifies sorting by username in case compromise type, compromise
  // time and origin are equal.
  test('sortCompromisedCredentialsByUsername', function() {
    const leakedPasswords = [
      makeCompromisedCredential(
          /*url*/ 'example.com', /*username*/ 'test0',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 0, /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'example.com', /*username*/ 'test1',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 1, /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'example.com', /*username*/ 'test2',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 2, /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'example.com', /*username*/ 'test3',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 3, /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'example.com', /*username*/ 'test4',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 4, /*elapsedMinSinceCompromise*/ 1),
      makeCompromisedCredential(
          /*url*/ 'example.com', /*username*/ 'test5',
          /*type*/ CompromiseType.LEAKED,
          /*id*/ 5, /*elapsedMinSinceCompromise*/ 1),
    ];
    const checkPasswordSection = createCheckPasswordSection();
    checkPasswordSection.updateCompromisedPasswordList(
        shuffleArray(leakedPasswords));
    flush();
    validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
  });

  test('startEqualsTrueSearchParameterStartsCheck', async function() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    createCheckPasswordSection();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(
        PasswordCheckInteraction.START_CHECK_AUTOMATICALLY, interaction);
    Router.getInstance().resetRouteForTesting();
  });

  // Verify clicking show password in menu reveal password.
  test('showHidePasswordMenuItemSuccess', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'jdoerrie',
        /*type*/ CompromiseType.LEAKED)];
    passwordManager.setPlaintextPassword('test4');
    const checkPasswordSection = createCheckPasswordSection();

    await passwordManager.whenCalled('getCompromisedCredentials');

    flush();
    const listElements = checkPasswordSection.$.leakedPasswordList;
    const node = listElements.children[0] as PasswordCheckListItemElement;
    assertEquals('password', node.$.insecurePassword.type);
    assertNotEquals('test4', node.$.insecurePassword.value);

    // Open the more actions menu and click 'Show Password'.
    node.$.more.click();
    checkPasswordSection.$.menuShowPassword.click();

    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');

    assertEquals(PasswordCheckInteraction.SHOW_PASSWORD, interaction);
    const {reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals(chrome.passwordsPrivate.PlaintextReason.VIEW, reason);
    assertEquals('text', node.$.insecurePassword.type);
    assertEquals('test4', node.$.insecurePassword.value);

    // Open the more actions menu and click 'Hide Password'.
    node.$.more.click();
    checkPasswordSection.$.menuShowPassword.click();

    assertEquals('password', node.$.insecurePassword.type);
    assertNotEquals('test4', node.$.insecurePassword.value);
  });

  // Verify if getPlaintext fails password will not be shown.
  test('showHidePasswordMenuItemFail', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'jdoerrie',
        /*type*/ CompromiseType.LEAKED)];
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');

    flush();
    const listElements = checkPasswordSection.$.leakedPasswordList;
    const node = listElements.children[0] as PasswordCheckListItemElement;
    assertEquals('password', node.$.insecurePassword.type);
    assertNotEquals('test4', node.$.insecurePassword.value);

    // Open the more actions menu and click 'Show Password'.
    node.$.more.click();
    checkPasswordSection.$.menuShowPassword.click();
    await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    // Verify that password field didn't change
    assertEquals('password', node.$.insecurePassword.type);
    assertNotEquals('test4', node.$.insecurePassword.value);
  });

  // Verify that clicking "Change password" reveals "Already changed password".
  test('alreadyChangedPassword', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'jdoerrie',
        /*type*/ CompromiseType.LEAKED)];
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');
    flush();
    const listElements = checkPasswordSection.$.leakedPasswordList;
    const passwordCheckListItem =
        listElements.children[0] as PasswordCheckListItemElement;

    const alreadyChanged =
        passwordCheckListItem.shadowRoot!.querySelector<HTMLElement>(
            '#alreadyChanged')!;
    assertFalse(isElementVisible(alreadyChanged));
    passwordCheckListItem.shadowRoot!
        .querySelector<HTMLElement>('#changePasswordButton')!.click();
    flush();
    assertTrue(isElementVisible(alreadyChanged));
  });

  // Verify if clicking "Edit password" in edit disclaimer opens edit dialog.
  test('testEditDisclaimer', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'jdoerrie',
        /*type*/ CompromiseType.LEAKED)];
    passwordManager.setPlaintextPassword('password');

    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');

    flush();
    const listElements = checkPasswordSection.$.leakedPasswordList;
    const node = listElements.children[0] as PasswordCheckListItemElement;
    // Clicking change password to show "Already changed password" link
    node.shadowRoot!.querySelector<HTMLElement>(
                        '#changePasswordButton')!.click();
    flush();
    // Clicking "Already changed password" to open edit disclaimer
    node.shadowRoot!.querySelector<HTMLElement>('#alreadyChanged')!.click();
    flush();

    const editDisclaimerDialog = checkPasswordSection.shadowRoot!.querySelector(
        'settings-password-edit-disclaimer-dialog')!;
    assertTrue(isElementVisible(editDisclaimerDialog));
    editDisclaimerDialog.$.edit.click();

    await passwordManager.whenCalled('requestPlaintextPassword');
    flush();
    assertTrue(isElementVisible(editDisclaimerDialog));
  });

  // <if expr="chromeos_ash">
  // Verify that getPlaintext succeeded after auth token resolved
  test('showHidePasswordMenuItemAuth', async function() {
    passwordManager.data.leakedCredentials = [makeCompromisedCredential(
        /*url*/ 'google.com', /*username*/ 'jdoerrie',
        /*type*/ CompromiseType.LEAKED)];
    const checkPasswordSection = createCheckPasswordSection();
    await passwordManager.whenCalled('getCompromisedCredentials');

    flush();
    const listElements = checkPasswordSection.$.leakedPasswordList;
    const node = listElements.children[0] as PasswordCheckListItemElement;

    // Open the more actions menu and click 'Show Password'.
    node.$.more.click();
    checkPasswordSection.$.menuShowPassword.click();
    // Wait for the more actions menu to disappear before proceeding.
    await eventToPromise('close', checkPasswordSection.$.moreActionsMenu);

    // Verify that password field didn't change
    assertEquals('password', node.$.insecurePassword.type);
    assertNotEquals('test4', node.$.insecurePassword.value);

    passwordManager.setPlaintextPassword('test4');
    node.tokenRequestManager.resolve();
    await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals('text', node.$.insecurePassword.type);
    assertEquals('test4', node.$.insecurePassword.value);
  });
  // </if>

  test('automaticallyCheckOnNavigationWhenFailEachTime', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.NO_PASSWORDS);
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    Router.getInstance().resetRouteForTesting();
    flush();
    passwordManager.resetResolver('startBulkPasswordCheck');
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    await passwordManager.whenCalled('startBulkPasswordCheck');
    assertFalse(section.startCheckAutomaticallySucceeded);
    Router.getInstance().resetRouteForTesting();
  });

  test('automaticallyCheckOnNavigationOnce', async function() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    const section = createCheckPasswordSection();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    Router.getInstance().resetRouteForTesting();
    flush();
    passwordManager.resetResolver('startBulkPasswordCheck');
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    assertTrue(section.startCheckAutomaticallySucceeded);
    Router.getInstance().resetRouteForTesting();
  });
});
