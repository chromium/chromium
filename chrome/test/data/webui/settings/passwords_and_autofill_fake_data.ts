// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManagerProxy, PasswordDialogMode, PasswordEditDialogElement, PasswordListItemElement, PasswordMoveMultiplePasswordsToAccountDialogElement, PasswordsExportDialogElement, PasswordsSectionElement, PaymentsManagerProxy, PersonalDataChangedListener} from 'chrome://settings/lazy_load.js';
import {MultiStoreExceptionEntry, MultiStorePasswordUiEntry, PasswordManagerProxy} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

export type PasswordEntryParams = {
  url?: string,
  username?: string,
  federationText?: string,
  id?: number,
  frontendId?: number,
  fromAccountStore?: boolean,
  note?: string,
};

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
  const frontendId = params.frontendId !== undefined ? params.frontendId : id;
  const fromAccountStore = params.fromAccountStore || false;
  const note = params.note || '';

  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    username: username,
    federationText: params.federationText,
    id: id,
    frontendId: frontendId,
    fromAccountStore: fromAccountStore,
    passwordNote: note,
  };
}

export type MultyStorePasswordEntryParams = {
  url?: string,
  username?: string,
  federationText?: string,
  accountId?: number,
  deviceId?: number,
  note?: string,
};

/**
 * Creates a multi-store password item with the same mock data as
 * createPasswordEntry(), so can be used for verifying deduplication result.
 * At least one of |params.accountId| and |params.deviceId| must be set.
 */
export function createMultiStorePasswordEntry(
    params: MultyStorePasswordEntryParams): MultiStorePasswordUiEntry {
  const dummyFrontendId = 42;
  let deviceEntry, accountEntry;
  if (params.deviceId !== undefined) {
    deviceEntry = createPasswordEntry({
      url: params.url,
      username: params.username,
      federationText: params.federationText,
      id: params.deviceId,
      frontendId: dummyFrontendId,
      fromAccountStore: false,
      note: params.note,
    });
  }
  if (params.accountId !== undefined) {
    accountEntry = createPasswordEntry({
      url: params.url,
      username: params.username,
      federationText: params.federationText,
      id: params.accountId,
      frontendId: dummyFrontendId,
      fromAccountStore: true,
      note: params.note,
    });
  }

  if (deviceEntry && accountEntry) {
    const mergedEntry = new MultiStorePasswordUiEntry(deviceEntry);
    mergedEntry.mergeInPlace(accountEntry);
    return mergedEntry;
  }
  if (deviceEntry) {
    return new MultiStorePasswordUiEntry(deviceEntry);
  }
  if (accountEntry) {
    return new MultiStorePasswordUiEntry(accountEntry);
  }

  assertNotReached();
  return new MultiStorePasswordUiEntry(createPasswordEntry());
}

export type ExceptionEntryParams = {
  url?: string,
  id?: number,
  frontendId?: number,
  fromAccountStore?: boolean,
};

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
  const frontendId = params.frontendId !== undefined ? params.frontendId : id;
  const fromAccountStore = params.fromAccountStore || false;
  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
    frontendId: frontendId,
    fromAccountStore: fromAccountStore,
  };
}

export type MultiStoreExceptionEntryParams = {
  url?: string,
  accountId?: number,
  deviceId?: number,
};

/**
 * Creates a multi-store password item with the same mock data as
 * createExceptionEntry(), so it can be used for verifying deduplication result.
 * At least one of |accountId| and |deviceId| must be set.
 */
export function createMultiStoreExceptionEntry(
    params: MultiStoreExceptionEntryParams): MultiStoreExceptionEntry {
  const dummyFrontendId = 42;
  let deviceEntry, accountEntry;
  if (params.deviceId !== undefined) {
    deviceEntry = createExceptionEntry({
      url: params.url,
      id: params.deviceId,
      frontendId: dummyFrontendId,
      fromAccountStore: false
    });
  }
  if (params.accountId !== undefined) {
    accountEntry = createExceptionEntry({
      url: params.url,
      id: params.accountId,
      frontendId: dummyFrontendId,
      fromAccountStore: true
    });
  }

  if (deviceEntry && accountEntry) {
    const mergedEntry = new MultiStoreExceptionEntry(deviceEntry);
    mergedEntry.mergeInPlace(accountEntry);
    return mergedEntry;
  }
  if (deviceEntry) {
    return new MultiStoreExceptionEntry(deviceEntry);
  }
  if (accountEntry) {
    return new MultiStoreExceptionEntry(accountEntry);
  }

  assertNotReached();
  return new MultiStoreExceptionEntry(createExceptionEntry());
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
    },
  };
}

/**
 * Creates a new insecure credential.
 */
export function makeInsecureCredential(
    url: string, username: string,
    id?: number): chrome.passwordsPrivate.InsecureCredential {
  return {
    id: id || 0,
    formattedOrigin: url,
    changePasswordUrl: `http://${url}/`,
    username: username,
    detailedOrigin: '',
    isAndroidCredential: false,
    signonRealm: '',
  };
}

/**
 * Creates a new compromised credential.
 */
export function makeCompromisedCredential(
    url: string, username: string, type: chrome.passwordsPrivate.CompromiseType,
    id?: number, elapsedMinSinceCompromise?: number,
    isMuted?: boolean): chrome.passwordsPrivate.InsecureCredential {
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
        }
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
    passwordListItem.entry = new MultiStorePasswordUiEntry(passwordEntry);
    this.document.body.appendChild(passwordListItem);
    flush();
    return passwordListItem;
  }

  /**
   * Helper method used to create a password editing dialog.
   */
  createPasswordEditDialog(
      passwordEntry: MultiStorePasswordUiEntry|null = null,
      passwords?: MultiStorePasswordUiEntry[],
      isAccountStoreUser: boolean = false,
      requestedDialogMode: PasswordDialogMode|
      null = null): PasswordEditDialogElement {
    const passwordDialog = this.document.createElement('password-edit-dialog');
    passwordDialog.existingEntry = passwordEntry;
    passwordDialog.requestedDialogMode = requestedDialogMode;
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
  createExportPasswordsDialog(passwordManager: PasswordManagerProxy):
      PasswordsExportDialogElement {
    passwordManager.requestExportProgressStatus = callback => {
      callback(chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
    };
    passwordManager.exportPasswords = (callback) => {
      callback();
    };

    const dialog = this.document.createElement('passwords-export-dialog');
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
  createMoveMultiplePasswordsDialog(passwordsToMove:
                                        MultiStorePasswordUiEntry[]):
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
