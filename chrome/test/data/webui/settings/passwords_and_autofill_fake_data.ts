// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManagerProxy, PasswordEditDialogElement, PasswordListItemElement, PasswordMoveMultiplePasswordsToAccountDialogElement, PasswordsExportDialogElement, PasswordsImportDialogElement, PasswordsSectionElement, PaymentsManagerProxy, PersonalDataChangedListener} from 'chrome://settings/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

export interface PasswordEntryParams {
  url?: string;
  username?: string;
  federationText?: string;
  id?: number;
  inAccountStore?: boolean;
  inProfileStore?: boolean;
  isAndroidCredential?: boolean;
  note?: string;
}

/**
 * Creates a single item for the list of passwords, in the format sent by the
 * password manager native code. If no |params.id| is passed, it is set to a
 * default, value so this should probably not be done in tests with multiple
 * entries (|params.id| is unique). If no |params.frontendId| is passed, it is
 * set to the same value set for |params.id|.
 */
export function createPasswordEntry(params?: PasswordEntryParams):
    chrome.passwordsPrivate.PasswordUiEntry {
  // Generate fake data if param is undefined.
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const username = params.username !== undefined ? params.username : 'user';
  const id = params.id !== undefined ? params.id : 42;
  // Fallback to device store if no parameter provided.
  let storeType: chrome.passwordsPrivate.PasswordStoreSet =
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE;

  if (params.inAccountStore && params.inProfileStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT;
  } else if (params.inAccountStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
  } else if (params.inProfileStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
  }
  const note = params.note || '';

  return {
    urls: {
      signonRealm: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    username: username,
    federationText: params.federationText,
    id: id,
    storedIn: storeType,
    isAndroidCredential: params.isAndroidCredential || false,
    note: note,
    password: '',
    hasStartableScript: false,
  };
}

export interface ExceptionEntryParams {
  url?: string;
  id?: number;
}

/**
 * Creates a single item for the list of password exceptions. If no |id| is
 * passed, it is set to a default, value so this should probably not be done in
 * tests with multiple entries (|id| is unique). If no |frontendId| is passed,
 * it is set to the same value set for |id|.
 */
export function createExceptionEntry(params?: ExceptionEntryParams):
    chrome.passwordsPrivate.ExceptionEntry {
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const id = params.id !== undefined ? params.id : 42;
  return {
    urls: {
      signonRealm: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
  };
}

export interface MultiStoreExceptionEntryParams {
  url?: string;
  accountId?: number;
  deviceId?: number;
}

/**
 * Creates a new fake address entry for testing.
 */
export function createEmptyAddressEntry(): chrome.autofillPrivate.AddressEntry {
  return {};
}

/**
 * Creates a fake address entry for testing.
 */
export function createAddressEntry(): chrome.autofillPrivate.AddressEntry {
  const fullName = 'John Doe';
  const addressLines = patternMaker('xxxx Main St', 10);
  return {
    guid: makeGuid(),
    fullNames: [fullName],
    companyName: 'Google',
    addressLines: addressLines,
    addressLevel1: 'CA',
    addressLevel2: 'Venice',
    postalCode: patternMaker('xxxxx', 10),
    countryCode: 'US',
    phoneNumbers: [patternMaker('(xxx) xxx-xxxx', 10)],
    emailAddresses: [patternMaker('userxxxx@gmail.com', 16)],
    languageCode: 'EN-US',
    metadata: {
      isLocal: true,
      summaryLabel: fullName,
      summarySublabel: ', ' + addressLines,
    },
  };
}

/**
 * Creates a new empty credit card entry for testing.
 */
export function createEmptyCreditCardEntry():
    chrome.autofillPrivate.CreditCardEntry {
  const now = new Date();
  const expirationMonth = now.getMonth() + 1;
  return {
    expirationMonth: expirationMonth.toString(),
    expirationYear: now.getFullYear().toString(),
  };
}

/**
 * Creates a new random credit card entry for testing.
 */
export function createCreditCardEntry():
    chrome.autofillPrivate.CreditCardEntry {
  const cards = ['Visa', 'Mastercard', 'Discover', 'Card'];
  const card = cards[Math.floor(Math.random() * cards.length)];
  const cardNumber = patternMaker('xxxx xxxx xxxx xxxx', 10);
  return {
    guid: makeGuid(),
    name: 'Jane Doe',
    cardNumber: cardNumber,
    expirationMonth: Math.ceil(Math.random() * 11).toString(),
    expirationYear: (2016 + Math.floor(Math.random() * 5)).toString(),
    network: `${card}_network`,
    metadata: {
      isLocal: true,
      summaryLabel: card + ' ' +
          '****' + cardNumber.substr(-4),
      summarySublabel: 'Jane Doe',
    },
  };
}

/**
 * Creates a new insecure credential.
 */
export function makeInsecureCredential(
    url: string, username: string,
    id?: number): chrome.passwordsPrivate.PasswordUiEntry {
  return {
    id: id || 0,
    storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
    changePasswordUrl: `http://${url}/`,
    hasStartableScript: false,
    urls: {
      signonRealm: `http://${url}/`,
      shown: url,
      link: `http://${url}/`,
    },
    username: username,
    note: '',
    isAndroidCredential: false,
  };
}

/**
 * Creates a new compromised credential.
 */
export function makeCompromisedCredential(
    url: string, username: string, type: chrome.passwordsPrivate.CompromiseType,
    id?: number, elapsedMinSinceCompromise?: number,
    isMuted?: boolean): chrome.passwordsPrivate.PasswordUiEntry {
  const credential = makeInsecureCredential(url, username, id);
  elapsedMinSinceCompromise = elapsedMinSinceCompromise || 0;
  credential.compromisedInfo = {
    compromiseTime: Date.now() - (elapsedMinSinceCompromise * 60000),
    elapsedTimeSinceCompromise: `${elapsedMinSinceCompromise} minutes ago`,
    compromiseType: type,
    isMuted: isMuted ?? false,
  };
  return credential;
}

/**
 * Creates a new password check status.
 */
export function makePasswordCheckStatus(
    state?: chrome.passwordsPrivate.PasswordCheckState, checked?: number,
    remaining?: number,
    lastCheck?: string): chrome.passwordsPrivate.PasswordCheckStatus {
  return {
    state: state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    alreadyProcessed: checked,
    remainingInQueue: remaining,
    elapsedTimeSinceLastCheck: lastCheck,
  };
}

/**
 * Creates a new random GUID for testing.
 */
function makeGuid(): string {
  return patternMaker('xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', 16);
}

/**
 * Replaces any 'x' in a string with a random number of the base.
 * @param pattern The pattern that should be used as an input.
 * @param base The number base. ie: 16 for hex or 10 for decimal.
 */
function patternMaker(pattern: string, base: number): string {
  return pattern.replace(/x/g, function() {
    return Math.floor(Math.random() * base).toString(base);
  });
}

/**
 * Helper class for creating password-section sub-element from fake data and
 * appending them to the document.
 */
export class PasswordSectionElementFactory {
  document: HTMLDocument;

  /**
   * @param document The test's |document| object.
   */
  constructor(document: HTMLDocument) {
    this.document = document;
  }

  /**
   * Helper method used to create a password section for the given lists.
   */
  createPasswordsSection(
      passwordManager: TestPasswordManagerProxy,
      passwordList: chrome.passwordsPrivate.PasswordUiEntry[],
      exceptionList: chrome.passwordsPrivate.ExceptionEntry[]):
      PasswordsSectionElement {
    // Override the TestPasswordManagerProxy data for testing.
    passwordManager.data.passwords = passwordList;
    passwordManager.data.exceptions = exceptionList;

    // Create a passwords-section to use for testing.
    const passwordsSection = this.document.createElement('passwords-section');
    passwordsSection.prefs = {
      credentials_enable_service: {},
      profile: {
        password_manager_leak_detection: {
          value: true,
        },
      },
    };
    this.document.body.appendChild(passwordsSection);
    flush();
    return passwordsSection;
  }

  /**
   * Helper method used to create a password list item.
   */
  createPasswordListItem(passwordEntry:
                             chrome.passwordsPrivate.PasswordUiEntry):
      PasswordListItemElement {
    const passwordListItem = this.document.createElement('password-list-item');
    passwordListItem.entry = passwordEntry;
    this.document.body.appendChild(passwordListItem);
    flush();
    return passwordListItem;
  }

  /**
   * Helper method used to create a password editing dialog.
   */
  createPasswordEditDialog(
      passwordEntry: chrome.passwordsPrivate.PasswordUiEntry|null = null,
      passwords?: chrome.passwordsPrivate.PasswordUiEntry[],
      isAccountStoreUser: boolean = false): PasswordEditDialogElement {
    const passwordDialog = this.document.createElement('password-edit-dialog');
    passwordDialog.existingEntry = passwordEntry;
    if (passwordEntry && !passwordEntry.federationText) {
      // Edit dialog is always opened with plaintext password for non-federated
      // credentials since user authentication is required before opening the
      // edit dialog.
      passwordDialog.existingEntry!.password = 'password';
    }
    passwordDialog.savedPasswords =
        passwords || (passwordEntry ? [passwordEntry] : []);
    passwordDialog.isAccountStoreUser = isAccountStoreUser;
    this.document.body.appendChild(passwordDialog);
    flush();
    return passwordDialog;
  }

  /**
   * Helper method used to create an export passwords dialog.
   */
  createExportPasswordsDialog(): PasswordsExportDialogElement {
    const dialog = this.document.createElement('passwords-export-dialog');
    this.document.body.appendChild(dialog);
    flush();
    return dialog;
  }

  /**
   * Helper method used to create a passwords import dialog.
   */
  createPasswordsImportDialog(
      isUserSyncingPasswords: boolean = false,
      isAccountStoreUser: boolean = false,
      accountEmail: string = ''): PasswordsImportDialogElement {
    const dialog = this.document.createElement('passwords-import-dialog');
    dialog.isUserSyncingPasswords = isUserSyncingPasswords;
    dialog.isAccountStoreUser = isAccountStoreUser;
    dialog.accountEmail = accountEmail;
    this.document.body.appendChild(dialog);
    flush();
    return dialog;
  }
}

/**
 * Helper class for creating password-device-section sub-element from fake data
 * and appending them to the document.
 */
export class PasswordDeviceSectionElementFactory {
  document: HTMLDocument;

  /**
   * @param document The test's |document| object.
   */
  constructor(document: HTMLDocument) {
    this.document = document;
  }

  /**
   * Helper method used to create a move multiple password to the Google Account
   * dialog.
   */
  createMoveMultiplePasswordsDialog(
      passwordsToMove: chrome.passwordsPrivate.PasswordUiEntry[]):
      PasswordMoveMultiplePasswordsToAccountDialogElement {
    const moveDialog = this.document.createElement(
        'password-move-multiple-passwords-to-account-dialog');
    moveDialog.passwordsToMove = passwordsToMove;
    this.document.body.appendChild(moveDialog);
    flush();
    return moveDialog;
  }
}

/** Helper class to track AutofillManager expectations. */
export class AutofillManagerExpectations {
  requestedAddresses: number = 0;
  listeningAddresses: number = 0;
  removeAddress: number = 0;
}

/**
 * Test implementation
 */
export class TestAutofillManager implements AutofillManagerProxy {
  private actual_: AutofillManagerExpectations;

  data: {
    addresses: chrome.autofillPrivate.AddressEntry[],
  };

  lastCallback:
      {setPersonalDataManagerListener: PersonalDataChangedListener|null};

  constructor() {
    this.actual_ = new AutofillManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      addresses: [],
    };

    // Holds the last callbacks so they can be called when needed.
    this.lastCallback = {
      setPersonalDataManagerListener: null,
    };
  }

  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    this.actual_.listeningAddresses++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  removePersonalDataManagerListener(_listener: PersonalDataChangedListener) {
    this.actual_.listeningAddresses--;
  }

  getAddressList(
      callback: (entries: chrome.autofillPrivate.AddressEntry[]) => void) {
    this.actual_.requestedAddresses++;
    callback(this.data.addresses);
  }

  saveAddress(_address: chrome.autofillPrivate.AddressEntry) {}

  removeAddress(_guid: string) {
    this.actual_.removeAddress++;
  }

  /**
   * Verifies expectations.
   */
  assertExpectations(expected: AutofillManagerExpectations) {
    const actual = this.actual_;
    assertEquals(expected.requestedAddresses, actual.requestedAddresses);
    assertEquals(expected.listeningAddresses, actual.listeningAddresses);
    assertEquals(expected.removeAddress, actual.removeAddress);
  }
}

/** Helper class to track PaymentsManager expectations. */
export class PaymentsManagerExpectations {
  requestedCreditCards: number = 0;
  listeningCreditCards: number = 0;
  requestedUpiIds: number = 0;
  removedCreditCards: number = 0;
  clearedCachedCreditCards: number = 0;
  addedVirtualCards: number = 0;
}

/**
 * Test implementation
 */
export class TestPaymentsManager implements PaymentsManagerProxy {
  private actual_: PaymentsManagerExpectations;
  private isUserVerifyingPlatformAuthenticatorAvailable_: boolean|null = null;

  data: {
    creditCards: chrome.autofillPrivate.CreditCardEntry[],
    upiIds: string[],
  };

  lastCallback:
      {setPersonalDataManagerListener: PersonalDataChangedListener|null};

  constructor() {
    this.actual_ = new PaymentsManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      creditCards: [],
      upiIds: [],
    };

    // Holds the last callbacks so they can be called when needed.
    this.lastCallback = {
      setPersonalDataManagerListener: null,
    };
  }

  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    this.actual_.listeningCreditCards++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  removePersonalDataManagerListener(_listener: PersonalDataChangedListener) {
    this.actual_.listeningCreditCards--;
  }

  getCreditCardList(
      callback: (entries: chrome.autofillPrivate.CreditCardEntry[]) => void) {
    this.actual_.requestedCreditCards++;
    callback(this.data.creditCards);
  }

  getUpiIdList(callback: (entries: string[]) => void) {
    this.actual_.requestedUpiIds++;
    callback(this.data.upiIds);
  }

  clearCachedCreditCard(_guid: string) {
    this.actual_.clearedCachedCreditCards++;
  }

  logServerCardLinkClicked() {}

  migrateCreditCards() {}

  removeCreditCard(_guid: string) {
    this.actual_.removedCreditCards++;
  }

  saveCreditCard(_creditCard: chrome.autofillPrivate.CreditCardEntry) {}

  setCreditCardFIDOAuthEnabledState(_enabled: boolean) {}

  addVirtualCard(_cardId: string) {
    this.actual_.addedVirtualCards++;
  }

  removeVirtualCard(_cardId: string) {}

  setIsUserVerifyingPlatformAuthenticatorAvailable(available: boolean|null) {
    this.isUserVerifyingPlatformAuthenticatorAvailable_ = available;
  }

  isUserVerifyingPlatformAuthenticatorAvailable() {
    return Promise.resolve(this.isUserVerifyingPlatformAuthenticatorAvailable_);
  }

  /**
   * Verifies expectations.
   */
  assertExpectations(expected: PaymentsManagerExpectations) {
    const actual = this.actual_;
    assertEquals(expected.requestedCreditCards, actual.requestedCreditCards);
    assertEquals(expected.listeningCreditCards, actual.listeningCreditCards);
    assertEquals(expected.removedCreditCards, actual.removedCreditCards);
    assertEquals(
        expected.clearedCachedCreditCards, actual.clearedCachedCreditCards);
    assertEquals(expected.addedVirtualCards, actual.addedVirtualCards);
  }
}
