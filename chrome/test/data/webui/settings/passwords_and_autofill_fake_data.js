// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillManager, PaymentsManager} from 'chrome://settings/lazy_load.js';
import {MultiStoreExceptionEntry, MultiStorePasswordUiEntry} from 'chrome://settings/settings.js';

import {assertEquals} from '../chai_assert.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
// clang-format on

/**
 * Creates a single item for the list of passwords, in the format sent by the
 * password manager native code. If no |id| is passed, it is set to a default,
 * value so this should probably not be done in tests with multiple entries
 * (|id| is unique). If no |frontendId| is passed, it is set to the same value
 * set for |id|.
 * @param {{ url: (string|undefined),
 *            username: (string|undefined),
 *            federationText: (string|undefined),
 *            id: (number|undefined),
 *            frontendId: (number|undefined),
 *            fromAccountStore: (boolean|undefined)
 *           }=} params
 * @return {chrome.passwordsPrivate.PasswordUiEntry}
 */
export function createPasswordEntry(params) {
  // Generate fake data if param is undefined.
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const username = params.username !== undefined ? params.username : 'user';
  const id = params.id !== undefined ? params.id : 42;
  const frontendId = params.frontendId !== undefined ? params.frontendId : id;
  const fromAccountStore = params.fromAccountStore || false;

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
  };
}

/**
 * Creates a multi-store password item with the same mock data as
 * createPasswordEntry(), so can be used for verifying deduplication result.
 * At least one of |accountId| and |deviceId| must be set.
 * @param {!{ url: (string|undefined),
 *            username: (string|undefined),
 *            federationText: (string|undefined),
 *            accountId: (number|undefined),
 *            deviceId: (number|undefined),
 *           }} params
 * @return {MultiStorePasswordUiEntry}
 */
export function createMultiStorePasswordEntry(params) {
  const dummyFrontendId = 42;
  let deviceEntry, accountEntry;
  if (params.deviceId !== undefined) {
    deviceEntry = createPasswordEntry({
      url: params.url,
      username: params.username,
      federationText: params.federationText,
      id: params.deviceId,
      frontendId: dummyFrontendId,
      fromAccountStore: false
    });
  }
  if (params.accountId !== undefined) {
    accountEntry = createPasswordEntry({
      url: params.url,
      username: params.username,
      federationText: params.federationText,
      id: params.accountId,
      frontendId: dummyFrontendId,
      fromAccountStore: true
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

/**
 * Creates a single item for the list of password exceptions. If no |id| is
 * passed, it is set to a default, value so this should probably not be done in
 * tests with multiple entries (|id| is unique). If no |frontendId| is passed,
 * it is set to the same value set for |id|.
 * @param {{ url: (string|undefined),
 *           id: (number|undefined),
 *           frontendId: (number|undefined),
 *           fromAccountStore: (boolean|undefined)
 *         }=} params
 * @return {chrome.passwordsPrivate.ExceptionEntry}
 */
export function createExceptionEntry(params) {
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

/**
 * Creates a multi-store password item with the same mock data as
 * createExceptionEntry(), so it can be used for verifying deduplication result.
 * At least one of |accountId| and |deviceId| must be set.
 * @param {!{ url: (string|undefined),
 *           accountId: (number|undefined),
 *           deviceId: (number|undefined),
 *         }} params
 * @return {MultiStoreExceptionEntry}
 */
export function createMultiStoreExceptionEntry(params) {
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
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
export function createEmptyAddressEntry() {
  return {};
}

/**
 * Creates a fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
export function createAddressEntry() {
  const ret = {};
  ret.guid = makeGuid_();
  ret.fullNames = ['John Doe'];
  ret.companyName = 'Google';
  ret.addressLines = patternMaker_('xxxx Main St', 10);
  ret.addressLevel1 = 'CA';
  ret.addressLevel2 = 'Venice';
  ret.postalCode = patternMaker_('xxxxx', 10);
  ret.countryCode = 'US';
  ret.phoneNumbers = [patternMaker_('(xxx) xxx-xxxx', 10)];
  ret.emailAddresses = [patternMaker_('userxxxx@gmail.com', 16)];
  ret.languageCode = 'EN-US';
  ret.metadata = {isLocal: true};
  ret.metadata.summaryLabel = ret.fullNames[0];
  ret.metadata.summarySublabel = ', ' + ret.addressLines;
  return ret;
}

/**
 * Creates a new empty credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
export function createEmptyCreditCardEntry() {
  const now = new Date();
  const expirationMonth = now.getMonth() + 1;
  const ret = {};
  ret.expirationMonth = expirationMonth.toString();
  ret.expirationYear = now.getFullYear().toString();
  return ret;
}

/**
 * Creates a new random credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
export function createCreditCardEntry() {
  const ret = {};
  ret.guid = makeGuid_();
  ret.name = 'Jane Doe';
  ret.cardNumber = patternMaker_('xxxx xxxx xxxx xxxx', 10);
  ret.expirationMonth = Math.ceil(Math.random() * 11).toString();
  ret.expirationYear = (2016 + Math.floor(Math.random() * 5)).toString();
  ret.metadata = {isLocal: true};
  const cards = ['Visa', 'Mastercard', 'Discover', 'Card'];
  const card = cards[Math.floor(Math.random() * cards.length)];
  ret.metadata.summaryLabel = card + ' ' +
      '****' + ret.cardNumber.substr(-4);
  return ret;
}

/**
 * Creates a new insecure credential.
 * @param {string} url
 * @param {string} username
 * @param {number=} id
 * @return {chrome.passwordsPrivate.InsecureCredential}
 * @private
 */
export function makeInsecureCredential(url, username, id) {
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
 * @param {string} url
 * @param {string} username
 * @param {chrome.passwordsPrivate.CompromiseType} type
 * @param {number=} id
 * @param {number=} elapsedMinSinceCompromise
 * @return {chrome.passwordsPrivate.InsecureCredential}
 * @private
 */
export function makeCompromisedCredential(
    url, username, type, id, elapsedMinSinceCompromise) {
  const credential = makeInsecureCredential(url, username, id);
  credential.compromisedInfo = {
    compromiseTime: Date.now() - (elapsedMinSinceCompromise * 60000),
    elapsedTimeSinceCompromise: `${elapsedMinSinceCompromise} minutes ago`,
    compromiseType: type,
  };
  return credential;
}

/**
 * Creates a new password check status.
 * @param {!chrome.passwordsPrivate.PasswordCheckState=} state
 * @param {number=} checked
 * @param {number=} remaining
 * @param {string=} lastCheck
 * @return {!chrome.passwordsPrivate.PasswordCheckStatus}
 */
export function makePasswordCheckStatus(state, checked, remaining, lastCheck) {
  return {
    state: state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    alreadyProcessed: checked,
    remainingInQueue: remaining,
    elapsedTimeSinceLastCheck: lastCheck,
  };
}

/**
 * Creates a new random GUID for testing.
 * @return {string}
 * @private
 */
function makeGuid_() {
  return patternMaker_('xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', 16);
}

/**
 * Replaces any 'x' in a string with a random number of the base.
 * @param {string} pattern The pattern that should be used as an input.
 * @param {number} base The number base. ie: 16 for hex or 10 for decimal.
 * @return {string}
 * @private
 */
function patternMaker_(pattern, base) {
  return pattern.replace(/x/g, function() {
    return Math.floor(Math.random() * base).toString(base);
  });
}

/**
 * Helper class for creating password-section sub-element from fake data and
 * appending them to the document.
 */
export class PasswordSectionElementFactory {
  /**
   * @param {HTMLDocument} document The test's |document| object.
   */
  constructor(document) {
    this.document = document;
  }

  /**
   * Helper method used to create a password section for the given lists.
   * @param {!TestPasswordManagerProxy} passwordManager
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @return {!Object}
   */
  createPasswordsSection(passwordManager, passwordList, exceptionList) {
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
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordEntry
   * @return {!Object}
   */
  createPasswordListItem(passwordEntry) {
    const passwordListItem = this.document.createElement('password-list-item');
    passwordListItem.entry = new MultiStorePasswordUiEntry(passwordEntry);
    passwordListItem.password = '';
    this.document.body.appendChild(passwordListItem);
    flush();
    return passwordListItem;
  }

  /**
   * Helper method used to create a password editing dialog.
   * @param {!MultiStorePasswordUiEntry} passwordEntry
   * @param {!Array<!MultiStorePasswordUiEntry>} passwords
   * @return {!Object}
   */
  createPasswordEditDialog(passwordEntry, passwords) {
    const passwordDialog = this.document.createElement('password-edit-dialog');
    passwordDialog.entry = passwordEntry;
    passwordDialog.password = '';
    passwordDialog.savedPasswords = passwords ? passwords : [];
    this.document.body.appendChild(passwordDialog);
    flush();
    return passwordDialog;
  }

  /**
   * Helper method used to create an export passwords dialog.
   * @return {!Object}
   */
  createExportPasswordsDialog(passwordManager) {
    passwordManager.requestExportProgressStatus = callback => {
      callback(chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
    };
    passwordManager.addPasswordsFileExportProgressListener = callback => {
      passwordManager.progressCallback = callback;
    };
    passwordManager.removePasswordsFileExportProgressListener = () => {};
    passwordManager.exportPasswords = (callback) => {
      callback();
    };

    const dialog = this.document.createElement('passwords-export-dialog');
    this.document.body.appendChild(dialog);
    flush();

    return dialog;
  }
}

/** Helper class to track AutofillManager expectations. */
export class AutofillManagerExpectations {
  constructor() {
    this.requestedAddresses = 0;
    this.listeningAddresses = 0;
  }
}

/**
 * Test implementation
 * @implements {AutofillManager}
 */
export class TestAutofillManager {
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

  /** @override */
  setPersonalDataManagerListener(listener) {
    this.actual_.listeningAddresses++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  /** @override */
  removePersonalDataManagerListener(listener) {
    this.actual_.listeningAddresses--;
  }

  /** @override */
  getAddressList(callback) {
    this.actual_.requestedAddresses++;
    callback(this.data.addresses);
  }

  /** @override */
  saveAddress() {}

  /** @override */
  removeAddress() {}

  /**
   * Verifies expectations.
   * @param {!AutofillManagerExpectations} expected
   */
  assertExpectations(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedAddresses, actual.requestedAddresses);
    assertEquals(expected.listeningAddresses, actual.listeningAddresses);
  }
}

/** Helper class to track PaymentsManager expectations. */
export class PaymentsManagerExpectations {
  constructor() {
    this.requestedCreditCards = 0;
    this.listeningCreditCards = 0;
    this.requestedUpiIds = 0;
  }
}

/**
 * Test implementation
 * @implements {PaymentsManager}
 */
export class TestPaymentsManager {
  constructor() {
    /** @private {!PaymentsManagerExpectations} */
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

  /** @override */
  setPersonalDataManagerListener(listener) {
    this.actual_.listeningCreditCards++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  /** @override */
  removePersonalDataManagerListener(listener) {
    this.actual_.listeningCreditCards--;
  }

  /** @override */
  getCreditCardList(callback) {
    this.actual_.requestedCreditCards++;
    callback(this.data.creditCards);
  }

  /** @override */
  getUpiIdList(callback) {
    this.actual_.requestedUpiIds++;
    callback(this.data.upiIds);
  }

  /** @override */
  clearCachedCreditCard() {}

  /** @override */
  logServerCardLinkClicked() {}

  /** @override */
  migrateCreditCards() {}

  /** @override */
  removeCreditCard() {}

  /** @override */
  saveCreditCard() {}

  /** @override */
  setCreditCardFIDOAuthEnabledState() {}

  /**
   * Verifies expectations.
   * @param {!PaymentsManagerExpectations} expected
   */
  assertExpectations(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedCreditCards, actual.requestedCreditCards);
    assertEquals(expected.listeningCreditCards, actual.listeningCreditCards);
  }
}
