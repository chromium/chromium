// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.


const COMPROMISE_TIME = 158322960000;

const ERROR_MESSAGE_FOR_CHANGE_PASSWORD =
    'Could not change the password. Either the password is empty, the user ' +
    'is not authenticated, vector of ids is empty or no matching password ' +
    'could be found at least for one of the ids.'

var availableTests = [
  function changeSavedPasswordSucceeds() {
    chrome.passwordsPrivate.changeSavedPassword(
        [0], 'new_user', 'new_pass', () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithIncorrectIdFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        [-1], 'new_user', 'new_pass', () => {
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithOneIncorrectIdFromArrayFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        [0, -1], 'new_user', 'new_pass', () => {
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithEmptyPasswordFails() {
    chrome.passwordsPrivate.changeSavedPassword([0], 'new_user', '', () => {
      chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
      chrome.test.succeed();
    });
  },

  function changeSavedPasswordWithEmptyArrayIdFails() {
    chrome.passwordsPrivate.changeSavedPassword([], 'new_user', '', () => {
      chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
      chrome.test.succeed();
    });
  },

  function removeAndUndoRemoveSavedPassword() {
    var numCalls = 0;
    var numSavedPasswords;
    var callback = function(savedPasswordsList) {
      numCalls++;

      if (numCalls == 1) {
        numSavedPasswords = savedPasswordsList.length;
        chrome.passwordsPrivate.removeSavedPassword(savedPasswordsList[0].id);
      } else if (numCalls == 2) {
        chrome.test.assertEq(savedPasswordsList.length, numSavedPasswords - 1);
        chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
      } else if (numCalls == 3) {
        chrome.test.assertEq(savedPasswordsList.length, numSavedPasswords);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function removeAndUndoRemoveSavedPasswordsBatch() {
    var numCalls = 0;
    var numSavedPasswords;

    var callback = function(savedPasswordsList) {
      numCalls++;

      if (numCalls == 1) {
        numSavedPasswords = savedPasswordsList.length;
        // There should be at least two passwords for this test to make sense.
        chrome.test.assertTrue(numSavedPasswords >= 2);
        chrome.passwordsPrivate.removeSavedPasswords(
            Array(savedPasswordsList[0].id, savedPasswordsList[1].id));
      } else if (numCalls == 2) {
        chrome.test.assertEq(savedPasswordsList.length, numSavedPasswords - 2);
        chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
      } else if (numCalls == 3) {
        chrome.test.assertEq(savedPasswordsList.length, numSavedPasswords);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function removeAndUndoRemovePasswordException() {
    var numCalls = 0;
    var numPasswordExceptions;
    var callback = function(passwordExceptionsList) {
      numCalls++;

      if (numCalls == 1) {
        numPasswordExceptions = passwordExceptionsList.length;
        chrome.passwordsPrivate.removePasswordException(
            passwordExceptionsList[0].id);
      } else if (numCalls == 2) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions - 1);
        chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
      } else if (numCalls == 3) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        callback);
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  function removeAndUndoRemovePasswordExceptionsBatch() {
    var numCalls = 0;
    var numPasswordExceptions;
    var callback = function(passwordExceptionsList) {
      numCalls++;

      if (numCalls == 1) {
        numPasswordExceptions = passwordExceptionsList.length;
        // There should be at least two exceptions for this test to make sense.
        chrome.test.assertTrue(numPasswordExceptions >= 2);
        chrome.passwordsPrivate.removePasswordExceptions(
            Array(passwordExceptionsList[0].id, passwordExceptionsList[1].id));
      } else if (numCalls == 2) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions - 2);
        chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
      } else if (numCalls == 3) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        callback);
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  function requestPlaintextPassword() {
    chrome.passwordsPrivate.requestPlaintextPassword(
        0, chrome.passwordsPrivate.PlaintextReason.VIEW, password => {
          // Ensure that the callback is invoked without an error state and the
          // expected plaintext password.
          chrome.test.assertNoLastError();
          chrome.test.assertEq('plaintext', password);
          chrome.test.succeed();
        });
  },

  function requestPlaintextPasswordFails() {
    chrome.passwordsPrivate.requestPlaintextPassword(
        123, chrome.passwordsPrivate.PlaintextReason.VIEW, password => {
          // Ensure that the callback is invoked with an error state and the
          // message contains the right id.
          chrome.test.assertLastError(
              'Could not obtain plaintext password. Either the user is not ' +
              'authenticated or no password with id = 123 could be found.');
          chrome.test.succeed();
        });
  },

  function getSavedPasswordList() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertTrue(list.length > 0);

      var idSet = new Set();
      for (var i = 0; i < list.length; ++i) {
        var entry = list[i];
        chrome.test.assertTrue(!!entry);
        chrome.test.assertTrue(!!entry.urls.origin);
        chrome.test.assertTrue(!!entry.urls.shown);
        chrome.test.assertTrue(!!entry.urls.link);
        idSet.add(entry.id);
      }

      // Ensure that all entry ids are unique.
      chrome.test.assertEq(list.length, idSet.size);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function getPasswordExceptionList() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertTrue(list.length > 0);

      var idSet = new Set();
      for (var i = 0; i < list.length; ++i) {
        var exception = list[i];
        chrome.test.assertTrue(!!exception.urls.origin);
        chrome.test.assertTrue(!!exception.urls.shown);
        chrome.test.assertTrue(!!exception.urls.link);
        idSet.add(exception.id);
      }

      // Ensure that all exception ids are unique.
      chrome.test.assertEq(list.length, idSet.size);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  function importPasswords() {
    chrome.passwordsPrivate.importPasswords();
    chrome.test.succeed();
  },

  function exportPasswords() {
    let callback = function() {
      chrome.test.assertNoLastError();

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.exportPasswords(callback);
  },

  function cancelExportPasswords() {
    chrome.passwordsPrivate.cancelExportPasswords();
    chrome.test.succeed();
  },

  function requestExportProgressStatus() {
    let callback = function(status) {
      chrome.test.assertEq(
          chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS, status);

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.requestExportProgressStatus(callback);
  },

  function isNotOptedInForAccountStorage() {
    var callback = function(optedIn) {
      chrome.test.assertEq(optedIn, false);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isOptedInForAccountStorage(callback);
  },

  function isOptedInForAccountStorage() {
    var callback = function(optedIn) {
      chrome.test.assertEq(optedIn, true);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isOptedInForAccountStorage(callback);
  },

  function optInForAccountStorage() {
    chrome.passwordsPrivate.optInForAccountStorage(true);
    chrome.passwordsPrivate.isOptedInForAccountStorage(function(optedIn) {
      chrome.test.assertEq(optedIn, true);
      chrome.test.succeed();
    });
  },

  function optOutForAccountStorage() {
    chrome.passwordsPrivate.optInForAccountStorage(false);
    chrome.passwordsPrivate.isOptedInForAccountStorage(function(optedIn) {
      chrome.test.assertEq(optedIn, false);
      chrome.test.succeed();
    });
  },

  function getCompromisedCredentials() {
    chrome.passwordsPrivate.getCompromisedCredentials(
        compromisedCredentials => {
          chrome.test.assertEq(1, compromisedCredentials.length);

          var compromisedCredential = compromisedCredentials[0];
          chrome.test.assertEq(
              'example.com', compromisedCredential.formattedOrigin);
          chrome.test.assertEq(
              'https://example.com', compromisedCredential.detailedOrigin);
          chrome.test.assertFalse(compromisedCredential.isAndroidCredential);
          chrome.test.assertEq(
              'https://example.com/change-password',
              compromisedCredential.changePasswordUrl);
          chrome.test.assertEq('alice', compromisedCredential.username);
          const compromiseTime =
              new Date(compromisedCredential.compromisedInfo.compromiseTime);
          chrome.test.assertEq(
              'Tue, 03 Mar 2020 12:00:00 GMT', compromiseTime.toUTCString());
          chrome.test.assertEq(
              '3 days ago',
              compromisedCredential.compromisedInfo.elapsedTimeSinceCompromise);
          chrome.test.assertEq(
              'LEAKED', compromisedCredential.compromisedInfo.compromiseType);
          chrome.test.succeed();
        });
  },

  function getWeakCredentials() {
    chrome.passwordsPrivate.getWeakCredentials(weakCredentials => {
      chrome.test.assertEq(1, weakCredentials.length);

      var weakredential = weakCredentials[0];
      chrome.test.assertEq('example.com', weakredential.formattedOrigin);
      chrome.test.assertEq('https://example.com', weakredential.detailedOrigin);
      chrome.test.assertFalse(weakredential.isAndroidCredential);
      chrome.test.assertEq(
          'https://example.com/change-password',
          weakredential.changePasswordUrl);
      chrome.test.assertEq('bob', weakredential.username);
      chrome.test.succeed();
    });
  },

  function getPlaintextInsecurePassword() {
    var compromisedCredential = {
      id: 0,
      formattedOrigin: 'example.com',
      detailedOrigin: 'https://example.com',
      isAndroidCredential: false,
      signonRealm: 'https://example.com',
      username: 'alice',
      compromisedInfo: {
        compromiseTime: COMPROMISE_TIME,
        elapsedTimeSinceCompromise: '3 days ago',
        compromiseType: 'LEAKED',
      },
    };

    chrome.passwordsPrivate.getPlaintextInsecurePassword(
        compromisedCredential, chrome.passwordsPrivate.PlaintextReason.VIEW,
        credentialWithPassword => {
          chrome.test.assertEq('plaintext', credentialWithPassword.password);
          chrome.test.succeed();
        });
  },

  function getPlaintextInsecurePasswordFails() {
    var compromisedCredential = {
      id: 0,
      formattedOrigin: 'example.com',
      detailedOrigin: 'https://example.com',
      isAndroidCredential: false,
      signonRealm: 'https://example.com',
      username: 'alice',
      compromisedInfo: {
        compromiseTime: COMPROMISE_TIME,
        elapsedTimeSinceCompromise: '3 days ago',
        compromiseType: 'LEAKED',
      },
    };

    chrome.passwordsPrivate.getPlaintextInsecurePassword(
        compromisedCredential, chrome.passwordsPrivate.PlaintextReason.VIEW,
        credentialWithPassword => {
          chrome.test.assertLastError(
              'Could not obtain plaintext insecure password. Either the user ' +
              'is not authenticated or no matching password could be found.');
          chrome.test.succeed();
        });
  },

  function changeInsecureCredentialWithEmptyPasswordFails() {
    chrome.passwordsPrivate.changeInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
          },
        },
        '', () => {
          chrome.test.assertLastError(
              'Could not change the insecure credential. The new password ' +
              'can\'t be empty.');
          chrome.test.succeed();
        });
  },

  function changeInsecureCredentialFails() {
    chrome.passwordsPrivate.changeInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
          },
        },
        'new_pass', () => {
          chrome.test.assertLastError(
              'Could not change the insecure credential. Either the user is ' +
              'not authenticated or no matching password could be found.');
          chrome.test.succeed();
        });
  },

  function changeInsecureCredentialSucceeds() {
    chrome.passwordsPrivate.changeInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
          },
        },
        'new_pass', () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function removeInsecureCredentialFails() {
    chrome.passwordsPrivate.removeInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
          },
        },
        () => {
          chrome.test.assertLastError(
              'Could not remove the insecure credential. Probably no ' +
              'matching password could be found.');
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function removeInsecureCredentialSucceeds() {
    chrome.passwordsPrivate.removeInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
          },
        },
        () => {
          chrome.test.assertNoLastError();
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function startPasswordCheck() {
    chrome.passwordsPrivate.startPasswordCheck(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function startPasswordCheckFailed() {
    chrome.passwordsPrivate.startPasswordCheck(() => {
      chrome.test.assertLastError('Starting password check failed.');
      chrome.test.succeed();
    });
  },

  function stopPasswordCheck() {
    chrome.passwordsPrivate.stopPasswordCheck(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function getPasswordCheckStatus() {
    chrome.passwordsPrivate.getPasswordCheckStatus(status => {
      chrome.test.assertEq('RUNNING', status.state);
      chrome.test.assertEq(5, status.alreadyProcessed);
      chrome.test.assertEq(10, status.remainingInQueue);
      chrome.test.assertEq('5 mins ago', status.elapsedTimeSinceLastCheck);
      chrome.test.succeed();
    });
  },

  function movePasswordsToAccount() {
    chrome.passwordsPrivate.movePasswordsToAccount([42]);
    chrome.test.succeed();
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
