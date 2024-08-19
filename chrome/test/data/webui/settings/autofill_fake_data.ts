// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {AutofillManagerProxy, PaymentsManagerProxy, PersonalDataChangedListener} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

const FieldType = chrome.autofillPrivate.FieldType;

export const STUB_USER_ACCOUNT_INFO: chrome.autofillPrivate.AccountInfo = {
  email: 'stub-user@example.com',
  isSyncEnabledForAutofillProfiles: false,
  isEligibleForAddressAccountStorage: false,
  isAutofillSyncToggleAvailable: false,
  isAutofillSyncToggleEnabled: false,
};

/**
 * Creates a new fake address entry for testing.
 */
export function createEmptyAddressEntry(): chrome.autofillPrivate.AddressEntry {
  return {
    fields: [],
  };
}

/**
 * Creates a fake address entry for testing.
 */
export function createAddressEntry(): chrome.autofillPrivate.AddressEntry {
  const fullName = 'John Doe';
  const addressLines = patternMaker('xxxx Main St', 10);
  return {
    guid: makeGuid(),
    fields: [
      {type: FieldType.NAME_FULL, value: fullName},
      {type: FieldType.COMPANY_NAME, value: 'Google'},
      {
        type: FieldType.ADDRESS_HOME_STREET_ADDRESS,
        value: addressLines,
      },
      {type: FieldType.ADDRESS_HOME_STATE, value: 'CA'},
      {type: FieldType.ADDRESS_HOME_CITY, value: 'Venice'},
      {
        type: FieldType.ADDRESS_HOME_ZIP,
        value: patternMaker('xxxxx', 10),
      },
      {type: FieldType.ADDRESS_HOME_COUNTRY, value: 'US'},
      {
        type: FieldType.PHONE_HOME_WHOLE_NUMBER,
        value: patternMaker('(xxx) xxx-xxxx', 10),
      },
      {
        type: FieldType.EMAIL_ADDRESS,
        value: patternMaker('userxxxx@gmail.com', 16),
      },
    ],
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
  const cardNumber = appendLuhnCheckBit(patternMaker('xxxxxxxxxxxxxxx', 10));
  const now = new Date();
  return {
    guid: makeGuid(),
    name: 'Jane Doe',
    cardNumber: cardNumber,
    expirationMonth: Math.ceil(Math.random() * 11).toString(),
    expirationYear:
        (now.getFullYear() + Math.floor(Math.random() * 5) + 1).toString(),
    network: `${card}_network`,
    imageSrc: 'chrome://theme/IDR_AUTOFILL_CC_GENERIC',
    metadata: {
      isLocal: true,
      summaryLabel: card + ' ' +
          '****' + cardNumber.substr(-4),
      summarySublabel: 'Jane Doe',
    },
  };
}

/**
 * Creates a new valid IBAN entry for testing.
 * If `value` is not empty, set guid when creating the IBAN entry, otherwise,
 * leave it undefined, as it is an empty IBAN.
 */
export function createIbanEntry(
    value?: string, nickname?: string): chrome.autofillPrivate.IbanEntry {
  return {
    guid: value ? makeGuid() : undefined,
    value: (value || value === '') ? value : 'CR99 0000 0000 0000 8888 88',
    nickname: (nickname || nickname === '') ? nickname : 'My doctor\'s IBAN',
    metadata: {
      isLocal: true,
      summaryLabel: ibanPatternMaker(value || 'CR99 0000 0000 0000 8888 88'),
      summarySublabel: nickname || 'My doctor\'s IBAN',
    },
  };
}

/**
 * Creates a new random GUID for testing.
 */
export function makeGuid(): string {
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
 * Calculates and appends a Luhn check bit for the given card number.
 * https://en.wikipedia.org/wiki/Luhn_algorithm
 * @param cardNumber The card number to calculate a check bit for
 */
function appendLuhnCheckBit(cardNumber: string): string {
  const digitsInReverse = cardNumber.split('').reverse();
  let sum = 0;
  let doubleDigit = true;
  for (const digit of digitsInReverse) {
    let intDigit = Number(digit);
    assertFalse(Number.isNaN(intDigit));
    if (doubleDigit) {
      intDigit *= 2;
      sum += Math.floor(intDigit / 10) + (intDigit % 10);
    } else {
      sum += intDigit;
    }
    doubleDigit = !doubleDigit;
  }

  const checkDigit = (10 - (sum % 10)) % 10;
  return cardNumber + checkDigit.toString();
}

/**
 * Converts value (E.g., CH12 1234 1234 1234 1234) of IBAN to a partially masked
 * text formatted by the following rules:
 * 1. Reveal the first and the last four characters.
 * 2. Mask the remaining digits.
 * 3. The identifier string will be arranged in groups of four with a space
 *    between each group.
 * Examples: BE71 0961 2345 6769 will be shown as: BE71 **** **** 6769.
 */
function ibanPatternMaker(ibanValue: string): string {
  let output = '';
  const strippedValue = ibanValue.replace(/\s/g, '');
  for (let i = 0; i < strippedValue.length; ++i) {
    if (i % 4 === 0 && i > 0) {
      output += ' ';
    }
    if (i < 4 || i >= strippedValue.length - 4) {
      output += strippedValue.charAt(i);
    } else {
      output += `*`;
    }
  }
  return output;
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
export class TestAutofillManager extends TestBrowserProxy implements
    AutofillManagerProxy {
  data: {
    addresses: chrome.autofillPrivate.AddressEntry[],
    accountInfo: chrome.autofillPrivate.AccountInfo,
  };

  lastCallback:
      {setPersonalDataManagerListener: PersonalDataChangedListener|null};

  constructor() {
    super([
      'getAccountInfo',
      'getAddressList',
      'removeAddress',
      'removePersonalDataManagerListener',
      'setPersonalDataManagerListener',
      'setAutofillSyncToggleEnabled',
    ]);

    // Set these to have non-empty data.
    this.data = {
      addresses: [],
      accountInfo: {
        email: 'stub-user@example.com',
        isSyncEnabledForAutofillProfiles: true,
        isEligibleForAddressAccountStorage: false,
        isAutofillSyncToggleAvailable: false,
        isAutofillSyncToggleEnabled: false,
      },
    };

    // Holds the last callbacks so they can be called when needed.
    this.lastCallback = {
      setPersonalDataManagerListener: null,
    };
  }

  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    this.methodCalled('setPersonalDataManagerListener');
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  removePersonalDataManagerListener(_listener: PersonalDataChangedListener) {
    this.methodCalled('removePersonalDataManagerListener');
  }

  getAccountInfo() {
    this.methodCalled('getAccountInfo');
    return Promise.resolve(this.data.accountInfo);
  }

  getAddressList() {
    this.methodCalled('getAddressList');
    return Promise.resolve(this.data.addresses);
  }

  saveAddress(_address: chrome.autofillPrivate.AddressEntry) {}

  removeAddress(_guid: string) {
    this.methodCalled('removeAddress');
  }

  setAutofillSyncToggleEnabled(_enabled: boolean) {
    this.methodCalled('setAutofillSyncToggleEnabled');
  }

  /**
   * Verifies expectations.
   */
  assertExpectations(expected: AutofillManagerExpectations) {
    assertEquals(
        expected.requestedAddresses, this.getCallCount('getAddressList'));
    assertEquals(
        expected.listeningAddresses,
        this.getCallCount('setPersonalDataManagerListener') -
            this.getCallCount('removePersonalDataManagerListener'));
    assertEquals(expected.removeAddress, this.getCallCount('removeAddress'));
  }
}

/** Helper class to track PaymentsManager expectations. */
export class PaymentsManagerExpectations {
  requestedCreditCards: number = 0;
  listeningCreditCards: number = 0;
  removedCreditCards: number = 0;
  addedVirtualCards: number = 0;
  requestedIbans: number = 0;
  removedIbans: number = 0;
  isValidIban: number = 0;
  authenticateUserAndFlipMandatoryAuthToggle: number = 0;
  getLocalCard: number = 0;
  bulkDeleteAllCvcs: number = 0;
}

/**
 * Test implementation
 */
export class TestPaymentsManager extends TestBrowserProxy implements
    PaymentsManagerProxy {
  private isValidIbanResult_: boolean = true;
  private isUserVerifyingPlatformAuthenticatorAvailable_: boolean|null = null;
  // <if expr="is_win or is_macosx">
  private isDeviceAuthAvailable_: boolean = false;
  // </if>

  data: {
    creditCards: chrome.autofillPrivate.CreditCardEntry[],
    ibans: chrome.autofillPrivate.IbanEntry[],
  };

  lastCallback:
      {setPersonalDataManagerListener: PersonalDataChangedListener|null};

  constructor() {
    super([
      'addVirtualCard',
      'authenticateUserAndFlipMandatoryAuthToggle',
      'bulkDeleteAllCvcs',
      'getCreditCardList',
      'getIbanList',
      'getLocalCard',
      'isValidIban',
      'removeCreditCard',
      'removeIban',
      'removePersonalDataManagerListener',
      'setPersonalDataManagerListener',
    ]);

    // Set these to have non-empty data.
    this.data = {
      creditCards: [],
      ibans: [],
    };

    // Holds the last callbacks so they can be called when needed.
    this.lastCallback = {
      setPersonalDataManagerListener: null,
    };
  }

  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    this.methodCalled('setPersonalDataManagerListener');
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  removePersonalDataManagerListener(_listener: PersonalDataChangedListener) {
    this.methodCalled('removePersonalDataManagerListener');
  }

  getCreditCardList() {
    this.methodCalled('getCreditCardList');
    return Promise.resolve(this.data.creditCards);
  }

  logServerCardLinkClicked() {}

  logServerIbanLinkClicked() {}

  migrateCreditCards() {}

  removeCreditCard(_guid: string) {
    this.methodCalled('removeCreditCard');
  }

  saveCreditCard(_creditCard: chrome.autofillPrivate.CreditCardEntry) {}

  addVirtualCard(_cardId: string) {
    this.methodCalled('addVirtualCard');
  }

  removeVirtualCard(_cardId: string) {}

  saveIban(_iban: chrome.autofillPrivate.IbanEntry) {}

  removeIban(_guid: string) {
    this.methodCalled('removeIban');
  }

  getIbanList() {
    this.methodCalled('getIbanList');
    return Promise.resolve(this.data.ibans);
  }

  setIsValidIban(isValidIbanResult: boolean) {
    this.isValidIbanResult_ = isValidIbanResult;
  }

  isValidIban(_ibanValue: string) {
    this.methodCalled('isValidIban');
    return Promise.resolve(this.isValidIbanResult_);
  }

  setIsUserVerifyingPlatformAuthenticatorAvailable(available: boolean|null) {
    this.isUserVerifyingPlatformAuthenticatorAvailable_ = available;
  }

  isUserVerifyingPlatformAuthenticatorAvailable() {
    return Promise.resolve(this.isUserVerifyingPlatformAuthenticatorAvailable_);
  }

  authenticateUserAndFlipMandatoryAuthToggle() {
    this.methodCalled('authenticateUserAndFlipMandatoryAuthToggle');
  }

  getLocalCard(_guid: string) {
    this.methodCalled('getLocalCard');
    const card =
        this.data.creditCards.find(creditCard => creditCard.guid === _guid);
    if (card !== undefined) {
      return Promise.resolve(card);
    }
    return Promise.resolve(null);
  }

  // <if expr="is_win or is_macosx">
  setIsDeviceAuthAvailable(available: boolean) {
    this.isDeviceAuthAvailable_ = available;
  }

  checkIfDeviceAuthAvailable() {
    return Promise.resolve(this.isDeviceAuthAvailable_);
  }
  // </if>

  bulkDeleteAllCvcs() {
    this.methodCalled('bulkDeleteAllCvcs');
  }

  /**
   * Verifies expectations.
   */
  assertExpectations(expected: PaymentsManagerExpectations) {
    assertEquals(
        expected.requestedCreditCards, this.getCallCount('getCreditCardList'),
        'requestedCreditCards mismatch');
    assertEquals(
        expected.listeningCreditCards,
        this.getCallCount('setPersonalDataManagerListener') -
            this.getCallCount('removePersonalDataManagerListener'),
        'listeningCreditCards mismatch');
    assertEquals(
        expected.removedCreditCards, this.getCallCount('removeCreditCard'),
        'removedCreditCards mismatch');
    assertEquals(
        expected.addedVirtualCards, this.getCallCount('addVirtualCard'),
        'addedVirtualCards mismatch');
    assertEquals(
        expected.requestedIbans, this.getCallCount('getIbanList'),
        'requestedIbans mismatch');
    assertEquals(
        expected.removedIbans, this.getCallCount('removeIban'),
        'removedIbans mismatch');
    assertEquals(
        expected.authenticateUserAndFlipMandatoryAuthToggle,
        this.getCallCount('authenticateUserAndFlipMandatoryAuthToggle'),
        'authenticateUserAndFlipMandatoryAuthToggle mismatch');
    assertEquals(
        expected.getLocalCard, this.getCallCount('getLocalCard'),
        'getLocalCard mismatch');
    assertEquals(
        expected.bulkDeleteAllCvcs, this.getCallCount('bulkDeleteAllCvcs'),
        'bulkDeleteAllCvcs mismatch');
  }
}
