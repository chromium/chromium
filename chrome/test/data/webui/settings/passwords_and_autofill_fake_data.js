// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used to create fake data for both passwords and autofill.
 * These sections are related, so it made sense to share this.
 */
function FakeDataMaker() {}

/**
 * Creates a single item for the list of passwords.
 * @param {string=} url
 * @param {string=} username
 * @param {number=} passwordLength
 * @param {number=} id
 * @return {chrome.passwordsPrivate.PasswordUiEntry}
 */
FakeDataMaker.passwordEntry = function(url, username, passwordLength, id) {
  // Generate fake data if param is undefined.
  url = url || FakeDataMaker.patternMaker_('www.xxxxxx.com', 16);
  username = username || FakeDataMaker.patternMaker_('user_xxxxx', 16);
  passwordLength = passwordLength || Math.floor(Math.random() * 15) + 3;
  id = id || 0;

  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    username: username,
    numCharactersInPassword: passwordLength,
    id: id,
  };
};

/**
 * Creates a single item for the list of password exceptions.
 * @param {string=} url
 * @param {number=} id
 * @return {chrome.passwordsPrivate.ExceptionEntry}
 */
FakeDataMaker.exceptionEntry = function(url, id) {
  url = url || FakeDataMaker.patternMaker_('www.xxxxxx.com', 16);
  id = id || 0;
  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
  };
};

/**
 * Creates a new fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
FakeDataMaker.emptyAddressEntry = function() {
  return {};
};

/**
 * Creates a fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
FakeDataMaker.addressEntry = function() {
  const ret = {};
  ret.guid = FakeDataMaker.makeGuid_();
  ret.fullNames = ['John Doe'];
  ret.companyName = 'Google';
  ret.addressLines = FakeDataMaker.patternMaker_('xxxx Main St', 10);
  ret.addressLevel1 = 'CA';
  ret.addressLevel2 = 'Venice';
  ret.postalCode = FakeDataMaker.patternMaker_('xxxxx', 10);
  ret.countryCode = 'US';
  ret.phoneNumbers = [FakeDataMaker.patternMaker_('(xxx) xxx-xxxx', 10)];
  ret.emailAddresses = [FakeDataMaker.patternMaker_('userxxxx@gmail.com', 16)];
  ret.languageCode = 'EN-US';
  ret.metadata = {isLocal: true};
  ret.metadata.summaryLabel = ret.fullNames[0];
  ret.metadata.summarySublabel = ', ' + ret.addressLines;
  return ret;
};

/**
 * Creates a new empty credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
FakeDataMaker.emptyCreditCardEntry = function() {
  const now = new Date();
  const expirationMonth = now.getMonth() + 1;
  const ret = {};
  ret.expirationMonth = expirationMonth.toString();
  ret.expirationYear = now.getFullYear().toString();
  return ret;
};

/**
 * Creates a new random credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
FakeDataMaker.creditCardEntry = function() {
  const ret = {};
  ret.guid = FakeDataMaker.makeGuid_();
  ret.name = 'Jane Doe';
  ret.cardNumber = FakeDataMaker.patternMaker_('xxxx xxxx xxxx xxxx', 10);
  ret.expirationMonth = Math.ceil(Math.random() * 11).toString();
  ret.expirationYear = (2016 + Math.floor(Math.random() * 5)).toString();
  ret.metadata = {isLocal: true};
  const cards = ['Visa', 'Mastercard', 'Discover', 'Card'];
  const card = cards[Math.floor(Math.random() * cards.length)];
  ret.metadata.summaryLabel = card + ' ' +
      '****' + ret.cardNumber.substr(-4);
  return ret;
};

/**
 * Creates a new random GUID for testing.
 * @return {string}
 * @private
 */
FakeDataMaker.makeGuid_ = function() {
  return FakeDataMaker.patternMaker_(
      'xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', 16);
};

/**
 * Replaces any 'x' in a string with a random number of the base.
 * @param {string} pattern The pattern that should be used as an input.
 * @param {number} base The number base. ie: 16 for hex or 10 for decimal.
 * @return {string}
 * @private
 */
FakeDataMaker.patternMaker_ = function(pattern, base) {
  return pattern.replace(/x/g, function() {
    return Math.floor(Math.random() * base).toString(base);
  });
};


/**
 * Helper class for creating password-section sub-element from fake data and
 * appending them to the document.
 */
class PasswordSectionElementFactory {
  /**
   * @param {HTMLDocument} document The test's |document| object.
   */
  constructor(document) {
    this.document = document;
  }

  /**
   * Helper method used to create a password section for the given lists.
   * @param {!PasswordManagerProxy} passwordManager
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @return {!Object}
   */
  createPasswordsSection(passwordManager, passwordList, exceptionList) {
    // Override the PasswordManagerProxy data for testing.
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
    Polymer.dom.flush();
    return passwordsSection;
  }

  /**
   * Helper method used to create a password list item.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordEntry
   * @return {!Object}
   */
  createPasswordListItem(passwordEntry) {
    const passwordListItem = this.document.createElement('password-list-item');
    passwordListItem.item = {entry: passwordEntry, password: ''};
    this.document.body.appendChild(passwordListItem);
    Polymer.dom.flush();
    return passwordListItem;
  }

  /**
   * Helper method used to create a password editing dialog.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordEntry
   * @return {!Object}
   */
  createPasswordEditDialog(passwordEntry) {
    const passwordDialog = this.document.createElement('password-edit-dialog');
    passwordDialog.item = {entry: passwordEntry, password: ''};
    this.document.body.appendChild(passwordDialog);
    Polymer.dom.flush();
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
    Polymer.dom.flush();

    if (cr.isChromeOS) {
      dialog.tokenRequestManager = new settings.BlockingRequestManager();
    }

    return dialog;
  }
}

/** @constructor */
function PasswordManagerExpectations() {
  this.requested = {
    passwords: 0,
    exceptions: 0,
    plaintextPassword: 0,
  };

  this.removed = {
    passwords: 0,
    exceptions: 0,
  };

  this.listening = {
    passwords: 0,
    exceptions: 0,
  };
}

/** Helper class to track AutofillManager expectations. */
class AutofillManagerExpectations {
  constructor() {
    this.requestedAddresses = 0;
    this.listeningAddresses = 0;
  }
}

/**
 * Test implementation
 * @implements {AutofillManager}
 * @constructor
 */
function TestAutofillManager() {
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

TestAutofillManager.prototype = {
  /** @override */
  setPersonalDataManagerListener: function(listener) {
    this.actual_.listeningAddresses++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  },

  /** @override */
  removePersonalDataManagerListener: function(listener) {
    this.actual_.listeningAddresses--;
  },

  /** @override */
  getAddressList: function(callback) {
    this.actual_.requestedAddresses++;
    callback(this.data.addresses);
  },

  /**
   * Verifies expectations.
   * @param {!AutofillManagerExpectations} expected
   */
  assertExpectations: function(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedAddresses, actual.requestedAddresses);
    assertEquals(expected.listeningAddresses, actual.listeningAddresses);
  },
};

/** Helper class to track PaymentsManager expectations. */
class PaymentsManagerExpectations {
  constructor() {
    this.requestedCreditCards = 0;
    this.listeningCreditCards = 0;
  }
}

/**
 * Test implementation
 * @implements {PaymentsManager}
 * @constructor
 */
function TestPaymentsManager() {
  this.actual_ = new PaymentsManagerExpectations();

  // Set these to have non-empty data.
  this.data = {
    creditCards: [],
  };

  // Holds the last callbacks so they can be called when needed.
  this.lastCallback = {
    setPersonalDataManagerListener: null,
  };
}

TestPaymentsManager.prototype = {
  /** @override */
  setPersonalDataManagerListener: function(listener) {
    this.actual_.listeningCreditCards++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  },

  /** @override */
  removePersonalDataManagerListener: function(listener) {
    this.actual_.listeningCreditCards--;
  },

  /** @override */
  getCreditCardList: function(callback) {
    this.actual_.requestedCreditCards++;
    callback(this.data.creditCards);
  },

  /**
   * Verifies expectations.
   * @param {!PaymentsManagerExpectations} expected
   */
  assertExpectations: function(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedCreditCards, actual.requestedCreditCards);
    assertEquals(expected.listeningCreditCards, actual.listeningCreditCards);
  },
};
