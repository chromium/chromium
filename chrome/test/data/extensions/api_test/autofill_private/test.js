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
var COUNTRY_CODE = 'ES';
var PHONE = '1 123-123-1234';
var EMAIL = 'johndoe@gmail.com';
var CARD_NAME = 'CardName';
var GUID = '1234-5678-90'
var MASKED_NUMBER = '1111';
var NUMBER = '4111 1111 1111 1111';
var EXP_MONTH = '02';
var EXP_YEAR = '2999';
var CVC = '987';
var MASKED_CVC = '•••';
var NICKNAME = 'nickname';
var IBAN_VALUE = 'AD1400080001001234567890';
var INVALID_IBAN_VALUE = 'AD14000800010012345678900';

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
  var UPDATED_IBAN_VALUE = 'AL35202111090000000001234567';

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

function updateCreditCardForCvc(updatedCvcValue) {
  // Reset onPersonalDataChanged.
  chrome.autofillPrivate.onPersonalDataChanged.removeListener(failOnceCalled);

  var UPDATED_CARD_NAME = 'UpdatedCardName';
  var UPDATED_EXP_YEAR = '2888';
  var UPDATED_NICKNAME = 'New nickname';

  function filterCardProperties(cards) {
    return cards.map(cards => {
      var filteredCards = {};
      ['guid', 'name', 'cardNumber', 'expirationMonth', 'expirationYear',
       'nickname', 'cvc']
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
                    cardNumber: MASKED_NUMBER,
                    expirationMonth: EXP_MONTH,
                    expirationYear: UPDATED_EXP_YEAR,
                    nickname: UPDATED_NICKNAME,
                    cvc: updatedCvcValue ? MASKED_CVC : undefined,
                  }],
                  filterCardProperties(cardList));
            }));

        // Update the card by saving a card with the same guid and using some
        // different information.
        chrome.autofillPrivate.saveCreditCard({
          guid: cardGuid,
          name: UPDATED_CARD_NAME,
          expirationYear: UPDATED_EXP_YEAR,
          nickname: UPDATED_NICKNAME,
          cvc: updatedCvcValue
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

    chrome.autofillPrivate.getCountryList(
        /*forAccountAddressProfile=*/ false, handler);
  },

  function getAddressComponents() {
    var COUNTRY_CODE = 'DE';

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
    function filterAddressProperties(address) {
      const fieldMap = {};
      address.fields.forEach(entry => {
        if (!!entry.value) {
          fieldMap[entry.type] = entry.value;
        }
      });
      return fieldMap;
    }

    chrome.autofillPrivate.getAddressList(
        chrome.test.callbackPass(function(addressList) {
          chrome.test.assertEq([], addressList);

          // Setup the callback that verifies that the address was correctly
          // added.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(1, addressList.length);
                const expectedAddress = {
                  NAME_FULL: NAME,
                  ADDRESS_HOME_STATE: ADDRESS_LEVEL1,
                  ADDRESS_HOME_CITY: ADDRESS_LEVEL2,
                  ADDRESS_HOME_DEPENDENT_LOCALITY: ADDRESS_LEVEL3,
                  ADDRESS_HOME_ZIP: POSTAL_CODE,
                  ADDRESS_HOME_SORTING_CODE: SORTING_CODE,
                  ADDRESS_HOME_COUNTRY: COUNTRY_CODE,
                  PHONE_HOME_WHOLE_NUMBER: PHONE,
                  EMAIL_ADDRESS: EMAIL,
                };
                const actualAddress = filterAddressProperties(addressList[0]);
                Object.keys(expectedAddress).forEach(key => {
                  chrome.test.assertEq(
                      expectedAddress[key], actualAddress[key]);
                })
              }));

          chrome.autofillPrivate.saveAddress({
            fields: [
              {
                type: chrome.autofillPrivate.FieldType.NAME_FULL,
                value: NAME
              },
              {
                type: chrome.autofillPrivate.FieldType.ADDRESS_HOME_STATE,
                value: ADDRESS_LEVEL1
              },
              {
                type: chrome.autofillPrivate.FieldType.ADDRESS_HOME_CITY,
                value: ADDRESS_LEVEL2
              },
              {
                type: chrome.autofillPrivate.FieldType
                          .ADDRESS_HOME_DEPENDENT_LOCALITY,
                value: ADDRESS_LEVEL3
              },
              {
                type: chrome.autofillPrivate.FieldType.ADDRESS_HOME_ZIP,
                value: POSTAL_CODE
              },
              {
                type: chrome.autofillPrivate.FieldType
                          .ADDRESS_HOME_SORTING_CODE,
                value: SORTING_CODE
              },
              {
                type:
                    chrome.autofillPrivate.FieldType.ADDRESS_HOME_COUNTRY,
                value: COUNTRY_CODE
              },
              {
                type: chrome.autofillPrivate.FieldType
                          .PHONE_HOME_WHOLE_NUMBER,
                value: PHONE
              },
              {
                type: chrome.autofillPrivate.FieldType.EMAIL_ADDRESS,
                value: EMAIL
              },
            ],
          });
        }));
  },

  function updateExistingAddress() {
    // The information that will be updated. It should be different than the
    // information in the addNewAddress function.
    var UPDATED_NAME = 'UpdatedName';
    var UPDATED_PHONE = '1 987-987-9876'

    function filterAddressProperties(address) {
      const filteredAddress = {guid: address.guid};
      address.fields.map(entry => {
        if (!!entry.value) {
          filteredAddress[entry.type] = entry.value
        }
      });
      return filteredAddress;
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
                chrome.test.assertEq(1, addressList.length);
                const expectedAddress = {
                  guid: addressGuid,
                  NAME_FULL: UPDATED_NAME,
                  ADDRESS_HOME_STATE: ADDRESS_LEVEL1,
                  ADDRESS_HOME_CITY: ADDRESS_LEVEL2,
                  ADDRESS_HOME_DEPENDENT_LOCALITY: ADDRESS_LEVEL3,
                  ADDRESS_HOME_ZIP: POSTAL_CODE,
                  ADDRESS_HOME_SORTING_CODE: SORTING_CODE,
                  ADDRESS_HOME_COUNTRY: COUNTRY_CODE,
                  PHONE_HOME_WHOLE_NUMBER: UPDATED_PHONE,
                  EMAIL_ADDRESS: EMAIL,
                };
                const actualAddress = filterAddressProperties(addressList[0]);
                Object.keys(expectedAddress).forEach(prop => {
                  chrome.test.assertEq(
                      expectedAddress[prop], actualAddress[prop]);
                })
              }));

          // Update the address by saving an address with the same guid and
          // using some different information.
          chrome.autofillPrivate.saveAddress({
            guid: addressGuid,
            fields: [
              {
                type: chrome.autofillPrivate.FieldType.NAME_FULL,
                value: UPDATED_NAME
              },
              {
                type: chrome.autofillPrivate.FieldType
                          .PHONE_HOME_WHOLE_NUMBER,
                value: UPDATED_PHONE
              },
            ],
          });
        }));
  },

  function addNewCreditCard() {
    function filterCardProperties(cards) {
      return cards.map(cards => {
        var filteredCards = {};
        ['name', 'cardNumber', 'expirationMonth', 'expirationYear', 'nickname',
         'cvc']
            .forEach(property => {
              filteredCards[property] = cards[property];
            });
        return filteredCards;
      });
    }

    function filterForAddedCard(cards) {
      return cards.filter(function (card) {
        // Credit cards are considered the same if they have a
        // matching card number, expiration month, and expiration
        // year.
        return card['cardNumber'] == MASKED_NUMBER &&
        card['expirationMonth'] == EXP_MONTH &&
        card['expirationYear'] == EXP_YEAR;
      })
    }

    chrome.autofillPrivate.getCreditCardList(
        chrome.test.callbackPass(function(cardList) {
          // Set up the callback that verifies that the card was correctly
          // added.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(
                    [{
                      name: CARD_NAME,
                      cardNumber: MASKED_NUMBER,
                      expirationMonth: EXP_MONTH,
                      expirationYear: EXP_YEAR,
                      nickname: NICKNAME,
                      cvc: MASKED_CVC
                    }],
                    filterCardProperties(filterForAddedCard(cardList)));
              }));

          chrome.autofillPrivate.saveCreditCard({
            name: CARD_NAME,
            cardNumber: NUMBER,
            expirationMonth: EXP_MONTH,
            expirationYear: EXP_YEAR,
            nickname: NICKNAME,
            cvc: CVC
          });
        }));
  },

  function addNewCreditCardWithoutCvc() {
    function filterCardProperties(cards) {
      return cards.map(cards => {
        var filteredCards = {};
        ['name', 'cardNumber', 'expirationMonth', 'expirationYear', 'nickname',
         'cvc']
            .forEach(property => {
              filteredCards[property] = cards[property];
            });
        return filteredCards;
      });
    }

    chrome.autofillPrivate.getCreditCardList(
        chrome.test.callbackPass(function(cardList) {
          chrome.test.assertEq([], cardList);

          // Set up the callback that verifies that the card was correctly
          // added.
          chrome.test.listenOnce(
              chrome.autofillPrivate.onPersonalDataChanged,
              chrome.test.callbackPass(function(addressList, cardList) {
                chrome.test.assertEq(
                    [{
                      name: CARD_NAME,
                      cardNumber: MASKED_NUMBER,
                      expirationMonth: EXP_MONTH,
                      expirationYear: EXP_YEAR,
                      nickname: undefined,
                      cvc: undefined
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
        expirationYear: EXP_YEAR,
        cvc: undefined
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
         'nickname', 'cvc']
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
                      cardNumber: MASKED_NUMBER,
                      expirationMonth: EXP_MONTH,
                      expirationYear: UPDATED_EXP_YEAR,
                      nickname: UPDATED_NICKNAME,
                      cvc: undefined
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

  function updateExistingCreditCard_CvcRemoved() {
    updateCreditCardForCvc(/*updatedCvcValue=*/ '');
  },

  function updateExistingCreditCard_CvcUpdated() {
    updateCreditCardForCvc(/*updatedCvcValue=*/ '123');
  },

  function updateExistingCreditCard_UnchangedCvc() {
    updateCreditCardForCvc(/*updatedCvcValue=*/ CVC);
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

  function removeExistingCard() {
    chrome.autofillPrivate.getCreditCardList(chrome.test.callbackPass(function(
        cardList) {
      // The card from the addNewCreditCard function should still be there.
      chrome.test.assertEq(1, cardList.length);
      var cardGuid = cardList[0].guid;

      // Set up the callback that verifies that the card was correctly
      // deleted.
      chrome.test.listenOnce(
          chrome.autofillPrivate.onPersonalDataChanged,
          chrome.test.callbackPass(function(addressList, cardList, ibanList) {
            chrome.test.assertEq(0, cardList.length);
          }));

      // Remove the card with the given guid.
      chrome.autofillPrivate.removeEntry(cardGuid);
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

  function isValidIban() {
    var handler1 = function(isValidIban) {
      // IBAN_VALUE should be valid, then follow up with invalid value.
      chrome.test.assertTrue(isValidIban);
      chrome.autofillPrivate.isValidIban(INVALID_IBAN_VALUE, handler2);
    }

    var handler2 = function(isValidIban) {
      // INVALID_IBAN_VALUE is not valid.
      chrome.test.assertFalse(isValidIban);
      chrome.test.succeed();
    }

    chrome.autofillPrivate.isValidIban(IBAN_VALUE, handler1);
  },

  function authenticateUserAndFlipMandatoryAuthToggle() {
    chrome.autofillPrivate.authenticateUserAndFlipMandatoryAuthToggle();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },

  function getLocalCard() {
    chrome.autofillPrivate.getCreditCardList(
        chrome.test.callbackPass(function(cardList) {
          // The card from the addNewCreditCard function should still be there.
          chrome.test.assertEq(1, cardList.length);
          var cardGuid = cardList[0].guid;

          // Get the card based on the `cardGuid` with unmasked card number.
          chrome.autofillPrivate.getLocalCard(
              cardGuid, chrome.test.callbackPass(function(card) {
                chrome.test.assertTrue(!!card);
                chrome.test.assertEq(
                    [{
                      guid: cardGuid,
                      cardNumber: NUMBER,
                      expirationMonth: EXP_MONTH,
                      expirationYear: EXP_YEAR,
                    }],
                    [{
                      guid: card.guid,
                      cardNumber: card.cardNumber,
                      expirationMonth: card.expirationMonth,
                      expirationYear: card.expirationYear,
                    }]);
                chrome.test.assertNoLastError();
              }));
        }));
  },

  function bulkDeleteAllCvcs() {
    chrome.autofillPrivate.bulkDeleteAllCvcs();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },

  function deleteAllUserAnnotationsEntries() {
    chrome.autofillPrivate.deleteAllUserAnnotationsEntries();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },


  function deleteUserAnnotationsEntry() {
    chrome.autofillPrivate.deleteUserAnnotationsEntry(123);
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },

  function getUserAnnotationsEntries() {
    chrome.autofillPrivate.getUserAnnotationsEntries();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },

];

/** @const */
var TESTS_FOR_CONFIG = {
  'addAndUpdateAddress': ['addNewAddress', 'updateExistingAddress'],
  'addAndUpdateCreditCard': [
    'addNewCreditCardWithoutCvc', 'noChangesToExistingCreditCard',
    'updateExistingCreditCard'
  ],
  'addAndUpdateCreditCard_AddCvc':
      ['addNewCreditCardWithoutCvc', 'updateExistingCreditCard_CvcUpdated'],
  'addAndUpdateCreditCard_RemoveCvc':
      ['addNewCreditCard', 'updateExistingCreditCard_CvcRemoved'],
  'addAndUpdateCreditCard_UpdateCvc':
      ['addNewCreditCard', 'updateExistingCreditCard_CvcUpdated'],
  'addAndUpdateCreditCard_UnchangedCvc':
      ['addNewCreditCard', 'updateExistingCreditCard_UnchangedCvc'],
  'addNewIbanNoNickname': ['addNewIbanNoNickname'],
  'addNewIbanWithNickname': ['addNewIbanWithNickname'],
  'noChangesToExistingIban':
      ['addNewIbanNoNickname', 'noChangesToExistingIban'],
  'updateExistingIbanNoNickname':
      ['addNewIbanNoNickname', 'updateExistingIban_NoNickname'],
  'updateExistingIbanWithNickname':
      ['addNewIbanNoNickname', 'updateExistingIban_WithNickname'],
  'removeExistingIban': ['addNewIbanNoNickname', 'removeExistingIban'],
  'removeExistingCard': ['addNewCreditCardWithoutCvc', 'removeExistingCard'],
  'removeExistingCard_WithCvcAndNickname':
      ['addNewCreditCard', 'removeExistingCard'],
  'isValidIban': ['isValidIban'],
  'authenticateUserAndFlipMandatoryAuthToggle':
      ['authenticateUserAndFlipMandatoryAuthToggle'],
  'getLocalCard': ['addNewCreditCard', 'getLocalCard'],
  'bulkDeleteAllCvcs': ['bulkDeleteAllCvcs'],
  'deleteAllUserAnnotationsEntries': ['deleteAllUserAnnotationsEntries'],
  'deleteUserAnnotationsEntries': ['deleteUserAnnotationsEntries'],
  'getUserAnnotationsEntries': ['getUserAnnotationsEntries'],
};

var testConfig = window.location.search.substring(1);
var testsToRun = TESTS_FOR_CONFIG[testConfig] || [testConfig];
chrome.test.runTests(availableTests.filter(function(op) {
  return testsToRun.includes(op.name);
}));
