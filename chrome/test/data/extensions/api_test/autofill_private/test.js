// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

// Constants for the tests.
var NAME = 'Name';
var COMPANY_NAME = 'Company name';
var ADDRESS_LEVEL1 = 'Address level 1';
var ADDRESS_LEVEL2 = 'Address level 2';
var ADDRESS_LEVEL3 = 'Address level 3';
var POSTAL_CODE = 'Postal code';
var SORTING_CODE = 'Sorting code';
var COUNTRY_CODE = 'US';
var PHONE = '1 123-123-1234';
var EMAIL = 'johndoe@gmail.com';
var CARD_NAME = 'CardName';
var NUMBER = '4111 1111 1111 1111';
var EXP_MONTH = '02';
var EXP_YEAR = '2999';
var IBAN_VALUE = 'AD14 0008 0001 0012 3456 7890';

var failOnceCalled = function() {
  chrome.test.fail();
};

function addNewIban(nickname) {
  function filterIbanProperties(ibans) {
    return ibans.map(iban => {
      var filteredIban = {};
      ['value', 'nickname'].forEach(property => {
        filteredIban[property] = iban[property];
      });
      return filteredIban;
    });
  }

  chrome.autofillPrivate.getIbanList(chrome.test.callbackPass(function(
      ibanList) {
    chrome.test.assertEq([], ibanList);

    // Set up the callback that verifies that the IBAN was correctly added.
    chrome.test.listenOnce(
        chrome.autofillPrivate.onPersonalDataChanged,
        chrome.test.callbackPass(function(addressList, cardList, ibanList) {
          chrome.test.assertEq(
              [{value: IBAN_VALUE, nickname: nickname}],
              filterIbanProperties(ibanList));
        }));

    chrome.autofillPrivate.saveIban({
      value: IBAN_VALUE,
      nickname: nickname
    });
  }));
};

function updateExistingIban(updatedNickname) {
  var UPDATED_IBAN_VALUE = 'AL35 2021 1109 0000 0000 0123 4567';

  function filterIbanProperties(ibans) {
    return ibans.map(iban => {
      var filteredIban = {};
      ['guid', 'value', 'nickname'].forEach(property => {
        filteredIban[property] = iban[property];
      });
      return filteredIban;
    });
  }

  chrome.autofillPrivate.getIbanList(chrome.test.callbackPass(function(
      ibanList) {
    // The IBAN from the addNewIban function should still be there.
    chrome.test.assertEq(1, ibanList.length);
    var ibanGuid = ibanList[0].guid;

    // Set up the callback that verifies that the IBAN was correctly
    // updated.
    chrome.test.listenOnce(
        chrome.autofillPrivate.onPersonalDataChanged,
        chrome.test.callbackPass(function(addressList, cardList, ibanList) {
          chrome.test.assertEq(
              [{
                guid: ibanGuid,
                value: UPDATED_IBAN_VALUE,
                nickname: updatedNickname
              }],
              filterIbanProperties(ibanList));
        }));

    // Update the IBAN by saving an IBAN with the same guid and using some
    // different information.
    chrome.autofillPrivate.saveIban({
      guid: ibanGuid,
      value: UPDATED_IBAN_VALUE,
      nickname: updatedNickname
    });
  }));
};

var availableTests = [
  function getCountryList() {
    var handler = function(countries) {
      var numSeparators = 0;
      var countBeforeSeparator = 0;
      var countAfterSeparator = 0;

      var beforeSeparator = true;

      chrome.test.assertTrue(countries.length > 1,
          'Expected more than one country');

      countries.forEach(function(country) {
        // Expecting to have both |name| and |countryCode| or neither.
        chrome.test.assertEq(!!country.name, !!country.countryCode);

        if (country.name) {
          if (beforeSeparator)
            ++countBeforeSeparator;
          else
            ++countAfterSeparator;
        } else {
          beforeSeparator = false;
          ++numSeparators;
        }
      });

      chrome.test.assertEq(1, numSeparators);
      chrome.test.assertEq(1, countBeforeSeparator);
      chrome.test.assertTrue(countAfterSeparator > 1,
          'Expected more than one country after the separator');

      chrome.test.succeed();
    };

    chrome.autofillPrivate.getCountryList(handler);
  },

  function getAddressComponents() {
    var COUNTRY_CODE = 'US';

    var handler = function(components) {
      chrome.test.assertTrue(!!components.components);
      chrome.test.assertTrue(!!components.components[0]);
      chrome.test.assertTrue(!!components.components[0].row);
      chrome.test.assertTrue(!!components.components[0].row[0]);
      chrome.test.assertTrue(!!components.components[0].row[0].field);
      chrome.test.assertTrue(!!components.components[0].row[0].fieldName);
      chrome.test.assertTrue(!!components.languageCode);
      chrome.test.succeed();
    }

    chrome.autofillPrivate.getAddressComponents(COUNTRY_CODE, handler);
  },

  function addNewAddress() {
    function filterAddressProperties(addresses) {
      return addresses.map(address => {
        var filteredAddress = {};
        ['fullNames', 'addressLevel1', 'addressLevel2', 'addressLevel3',
         'postalCode', 'sortingCode', 'phoneNumbers', 'emailAddresses']
            .forEach(property => {
              filteredAddress[property] = address[property];
            });
        return filteredAddress;
      });
    }

    chrome.autofillPrivate.getAddressList(
        chrome.test.callbackPass(function(addressList) {
          chrome.test.assertEq([], addressList);

          // Setup the callback that verifies that the address was correctly
          // added.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(
                    [{
                      fullNames: [NAME],
                      addressLevel1: ADDRESS_LEVEL1,
                      addressLevel2: ADDRESS_LEVEL2,
                      addressLevel3: ADDRESS_LEVEL3,
                      postalCode: POSTAL_CODE,
                      sortingCode: SORTING_CODE,
                      phoneNumbers: [PHONE],
                      emailAddresses: [EMAIL]
                    }],
                    filterAddressProperties(addressList));
              }));

          chrome.autofillPrivate.saveAddress({
            fullNames: [NAME],
            addressLevel1: ADDRESS_LEVEL1,
            addressLevel2: ADDRESS_LEVEL2,
            addressLevel3: ADDRESS_LEVEL3,
            postalCode: POSTAL_CODE,
            sortingCode: SORTING_CODE,
            countryCode: COUNTRY_CODE,
            phoneNumbers: [PHONE],
            emailAddresses: [EMAIL]
          });
        }));
  },

  function updateExistingAddress() {
    // The information that will be updated. It should be different than the
    // information in the addNewAddress function.
    var UPDATED_NAME = 'UpdatedName';
    var UPDATED_PHONE = '1 987-987-9876'

    function filterAddressProperties(addresses) {
      return addresses.map(address => {
        var filteredAddress = {};
        ['guid', 'fullNames', 'addressLevel1', 'addressLevel2', 'addressLevel3',
         'postalCode', 'sortingCode', 'phoneNumbers', 'emailAddresses']
            .forEach(property => {
              filteredAddress[property] = address[property];
            });
        return filteredAddress;
      });
    }

    chrome.autofillPrivate.getAddressList(
        chrome.test.callbackPass(function(addressList) {
          // The address from the addNewAddress function should still be there.
          chrome.test.assertEq(1, addressList.length);
          var addressGuid = addressList[0].guid;

          // Setup the callback that verifies that the address was correctly
          // updated.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(
                    [{
                      guid: addressGuid,
                      fullNames: [UPDATED_NAME],
                      addressLevel1: ADDRESS_LEVEL1,
                      addressLevel2: ADDRESS_LEVEL2,
                      addressLevel3: ADDRESS_LEVEL3,
                      postalCode: POSTAL_CODE,
                      sortingCode: SORTING_CODE,
                      phoneNumbers: [UPDATED_PHONE],
                      emailAddresses: [EMAIL]
                    }],
                    filterAddressProperties(addressList));
              }));

          // Update the address by saving an address with the same guid and
          // using some different information.
          chrome.autofillPrivate.saveAddress({
            guid: addressGuid,
            fullNames: [UPDATED_NAME],
            phoneNumbers: [UPDATED_PHONE]
          });
        }));
  },

  function addNewCreditCard() {
    function filterCardProperties(cards) {
      return cards.map(cards => {
        var filteredCards = {};
        ['name', 'cardNumber', 'expirationMonth', 'expirationYear', 'nickname']
            .forEach(property => {
              filteredCards[property] = cards[property];
            });
        return filteredCards;
      });
    }

    chrome.autofillPrivate.getCreditCardList(
        chrome.test.callbackPass(function(cardList) {
          chrome.test.assertEq([], cardList);

          // Setup the callback that verifies that the card was correctly added.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(
                    [{
                      name: CARD_NAME,
                      cardNumber: NUMBER,
                      expirationMonth: EXP_MONTH,
                      expirationYear: EXP_YEAR,
                      nickname: undefined
                    }],
                    filterCardProperties(cardList));
              }));

          chrome.autofillPrivate.saveCreditCard({
            name: CARD_NAME,
            cardNumber: NUMBER,
            expirationMonth: EXP_MONTH,
            expirationYear: EXP_YEAR
          });
        }));
  },

  function noChangesToExistingCreditCard() {
    chrome.autofillPrivate.getCreditCardList(chrome.test.callbackPass(function(
        cardList) {
      // The card from the addNewCreditCard function should still be there.
      chrome.test.assertEq(1, cardList.length);
      var cardGuid = cardList[0].guid;

      // Set up the listener that verifies that onPersonalDataChanged shouldn't
      // be called.
      chrome.autofillPrivate.onPersonalDataChanged.addListener(failOnceCalled);

      // Save the card with the same info, shouldn't invoke
      // onPersonalDataChanged.
      chrome.autofillPrivate.saveCreditCard({
        guid: cardGuid,
        name: CARD_NAME,
        cardNumber: NUMBER,
        expirationMonth: EXP_MONTH,
        expirationYear: EXP_YEAR
      });
    }));
  },

  function updateExistingCreditCard() {
    // Reset onPersonalDataChanged.
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(failOnceCalled);

    var UPDATED_CARD_NAME = 'UpdatedCardName';
    var UPDATED_EXP_YEAR = '2888';
    var UPDATED_NICKNAME = 'New nickname';

    function filterCardProperties(cards) {
      return cards.map(cards => {
        var filteredCards = {};
        ['guid', 'name', 'cardNumber', 'expirationMonth', 'expirationYear',
         'nickname']
            .forEach(property => {
              filteredCards[property] = cards[property];
            });
        return filteredCards;
      });
    }

    chrome.autofillPrivate.getCreditCardList(
        chrome.test.callbackPass(function(cardList) {
          // The card from the addNewCreditCard function should still be there.
          chrome.test.assertEq(1, cardList.length);
          var cardGuid = cardList[0].guid;

          // Set up the callback that verifies that the card was correctly
          // updated.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(
                    [{
                      guid: cardGuid,
                      name: UPDATED_CARD_NAME,
                      cardNumber: NUMBER,
                      expirationMonth: EXP_MONTH,
                      expirationYear: UPDATED_EXP_YEAR,
                      nickname: UPDATED_NICKNAME
                    }],
                    filterCardProperties(cardList));
              }));

          // Update the card by saving a card with the same guid and using some
          // different information.
          chrome.autofillPrivate.saveCreditCard({
            guid: cardGuid,
            name: UPDATED_CARD_NAME,
            expirationYear: UPDATED_EXP_YEAR,
            nickname: UPDATED_NICKNAME
          });
        }));
  },

  function addNewIbanNoNickname() {
    addNewIban(/*nickname=*/undefined);
  },

  function addNewIbanWithNickname() {
    addNewIban(/*nickname=*/'nickname');
  },

  function noChangesToExistingIban() {
    chrome.autofillPrivate.getIbanList(chrome.test.callbackPass(function(
        ibanList) {
      // The IBAN from the addNewIban function should still be there.
      chrome.test.assertEq(1, ibanList.length);
      var ibanGuid = ibanList[0].guid;

      // Set up the listener that verifies that onPersonalDataChanged shouldn't
      // be called.
      chrome.autofillPrivate.onPersonalDataChanged.addListener(failOnceCalled);

      // Save the IBAN with the same info, shouldn't invoke
      // onPersonalDataChanged.
      chrome.autofillPrivate.saveIban({
        guid: ibanGuid,
        value: IBAN_VALUE,
      });

      // Reset onPersonalDataChanged.
      chrome.autofillPrivate.onPersonalDataChanged.removeListener(
          failOnceCalled);
    }));
  },

  function updateExistingIban_NoNickname() {
    updateExistingIban(/*updatedNickname=*/undefined);
  },

  function updateExistingIban_WithNickname() {
    updateExistingIban(/*updatedNickname=*/'New nickname');
  },

  function removeExistingIban() {
    chrome.autofillPrivate.getIbanList(chrome.test.callbackPass(function(
        ibanList) {
      // The IBAN from the addNewIban function should still be there.
      chrome.test.assertEq(1, ibanList.length);
      var ibanGuid = ibanList[0].guid;

      // Set up the callback that verifies that the IBAN was correctly
      // updated.
      chrome.test.listenOnce(
          chrome.autofillPrivate.onPersonalDataChanged,
          chrome.test.callbackPass(function(addressList, cardList, ibanList) {
            chrome.test.assertEq(0, ibanList.length);
          }));

      // Remove the IBAN with the given guid.
      chrome.autofillPrivate.removeEntry(ibanGuid);
    }));
  },

  function removeEntry() {
    var guid;

    var numCalls = 0;
    var getCardsHandler = function(creditCardList) {
      numCalls++;
      chrome.test.assertEq(1, numCalls);
    }

    var personalDataChangedHandler = function(addressList, creditCardList) {
      numCalls++;

      if (numCalls == 2) {
        chrome.test.assertEq(creditCardList.length, 1);
        var creditCard = creditCardList[0];
        chrome.test.assertEq(creditCard.name, NAME);

        guid = creditCard.guid;
        chrome.autofillPrivate.removeEntry(guid);
      } else if (numCalls == 3) {
        chrome.test.assertEq(creditCardList.length, 0);
        chrome.test.succeed();
      } else {
        // We should never receive such a call.
        chrome.test.fail();
      }
    }

    chrome.autofillPrivate.onPersonalDataChanged.addListener(
        personalDataChangedHandler);
    chrome.autofillPrivate.getCreditCardList(getCardsHandler);
    chrome.autofillPrivate.saveCreditCard({name: NAME});
  },

  function validatePhoneNumbers() {
    var COUNTRY_CODE = 'US';
    var FAKE_NUMBER = '1-800-123-4567';
    var ORIGINAL_NUMBERS = [FAKE_NUMBER];
    var FIRST_NUMBER_TO_ADD = '1-800-234-5768';
    // Same as original number, but without formatting.
    var SECOND_NUMBER_TO_ADD = '18001234567';

    var handler1 =
        function(validateNumbers) {
      chrome.test.assertEq(validateNumbers.length, 1);
      chrome.test.assertEq(FAKE_NUMBER, validateNumbers[0]);

      chrome.autofillPrivate.validatePhoneNumbers(
          {
            phoneNumbers: validateNumbers.concat(FIRST_NUMBER_TO_ADD),
            indexOfNewNumber: 1,  // A new number (FIRST_NUMBER_TO_ADD) is added
                                  // at the end of the list.
            countryCode: COUNTRY_CODE
          },
          handler2);
    }

    var handler2 = function(validateNumbers) {
      chrome.test.assertEq(validateNumbers.length, 2);
      chrome.test.assertEq(FAKE_NUMBER, validateNumbers[0]);
      chrome.test.assertEq(FIRST_NUMBER_TO_ADD, validateNumbers[1]);

      chrome.autofillPrivate.validatePhoneNumbers(
          {
            phoneNumbers: validateNumbers.concat(SECOND_NUMBER_TO_ADD),
            indexOfNewNumber: 2,  // A new number (SECOND_NUMBER_TO_ADD) is
                                  // added at the end of the list.
            countryCode: COUNTRY_CODE
          },
          handler3);
    };

    var handler3 = function(validateNumbers) {
      // Newly-added number should not appear since it was the same as an
      // existing number.
      chrome.test.assertEq(validateNumbers.length, 2);
      chrome.test.assertEq(FAKE_NUMBER, validateNumbers[0]);
      chrome.test.assertEq(FIRST_NUMBER_TO_ADD, validateNumbers[1]);
      chrome.test.succeed();
    }

    chrome.autofillPrivate.validatePhoneNumbers({
      phoneNumbers: ORIGINAL_NUMBERS,
      indexOfNewNumber: 0,
      countryCode: COUNTRY_CODE
    }, handler1);
  },
];

/** @const */
var TESTS_FOR_CONFIG = {
  'addAndUpdateAddress': ['addNewAddress', 'updateExistingAddress'],
  'addAndUpdateCreditCard': [
    'addNewCreditCard', 'noChangesToExistingCreditCard',
    'updateExistingCreditCard'
  ],
  'addNewIbanNoNickname': ['addNewIbanNoNickname'],
  'addNewIbanWithNickname': ['addNewIbanWithNickname'],
  'noChangesToExistingIban': ['addNewIbanNoNickname','noChangesToExistingIban'],
  'updateExistingIbanNoNickname': [
    'addNewIbanNoNickname', 'updateExistingIban_NoNickname'],
  'updateExistingIbanWithNickname': [
    'addNewIbanNoNickname', 'updateExistingIban_WithNickname'],
  'removeExistingIban': [
    'addNewIbanNoNickname', 'removeExistingIban'],
};

chrome.test.getConfig(function(config) {
  var testConfig = config.customArg;
  var testsToRun = TESTS_FOR_CONFIG[testConfig] || [testConfig];
  chrome.test.runTests(availableTests.filter(function(op) {
    return testsToRun.includes(op.name);
  }));
});
