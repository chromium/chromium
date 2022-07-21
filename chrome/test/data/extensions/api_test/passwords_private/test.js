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
  function isAccountStoreDefaultWhenFalse() {
    chrome.passwordsPrivate.isAccountStoreDefault(isDefault => {
      chrome.test.assertNoLastError();
      chrome.test.assertFalse(isDefault);
      chrome.test.succeed();
    });
  },

  function isAccountStoreDefaultWhenTrue() {
    chrome.passwordsPrivate.isAccountStoreDefault(isDefault => {
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(isDefault);
      chrome.test.succeed();
    });
  },

  function getUrlCollectionWhenUrlValidSucceeds() {
    chrome.passwordsPrivate.getUrlCollection(
        'https://example.com', urlCollection => {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(!!urlCollection);
          chrome.test.succeed();
        });
  },

  function getUrlCollectionWhenUrlInvalidFails() {
    chrome.passwordsPrivate.getUrlCollection('', () => {
      chrome.test.assertLastError(
          'Provided string doesn\'t meet password URL requirements. ' +
          'Either the format is invalid or the scheme is not unsupported.');
      chrome.test.succeed();
    });
  },

  function addPasswordWhenOperationSucceeds() {
    chrome.passwordsPrivate.addPassword(
        /* @type {chrome.passwordsPrivate.AddPasswordOptions} */
        {
          url: 'https://example.com',
          username: 'username',
          password: 'password',
          note: '',
          useAccountStore: false
        },
        () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function addPasswordWhenOperationFails() {
    chrome.passwordsPrivate.addPassword(
        /* @type {chrome.passwordsPrivate.AddPasswordOptions} */
        {
          url: 'https://example.com',
          username: 'username',
          password: '',
          note: '',
          useAccountStore: true
        },
        () => {
          chrome.test.assertLastError(
              'Could not add the password. Either the url is invalid, the ' +
              'password is empty or an entry with such origin and username ' +
              'already exists.');
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordSucceeds() {
    chrome.passwordsPrivate.changeSavedPassword(
        [0], {username: 'new_user', password: 'new_pass'}, (credentialIds) => {
          chrome.test.assertEq({deviceId: 0}, credentialIds);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithIncorrectIdFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        [-1], {username: 'new_user', password: 'new_pass'}, (credentialIds) => {
          chrome.test.assertEq(undefined, credentialIds);
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithOneIncorrectIdFromArrayFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        [0, -1], {username: 'new_user', password: 'new_pass'},
        (credentialIds) => {
          chrome.test.assertEq(undefined, credentialIds);
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithEmptyPasswordFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        [0], {username: 'new_user', password: ''}, (credentialIds) => {
          chrome.test.assertEq(undefined, credentialIds);
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithEmptyArrayIdFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        [], {username: 'new_user', password: ''}, (credentialIds) => {
          chrome.test.assertEq(undefined, credentialIds);
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithNoteSucceeds() {
    chrome.passwordsPrivate.changeSavedPassword(
        [0], {username: 'new_user', password: 'new_pass', note: 'some note'},
        (credentialIds) => {
          chrome.test.assertEq({deviceId: 0}, credentialIds);
          chrome.test.assertNoLastError();
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
        chrome.passwordsPrivate.removeSavedPassword(savedPasswordsList[0].id,
            chrome.passwordsPrivate.PasswordStoreSet.DEVICE);
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
    let callback = function(importResults) {
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(!!importResults);
      chrome.test.succeed();
    };
    chrome.passwordsPrivate.importPasswords(
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
      callback);
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
      hasStartableScript: false,
      signonRealm: 'https://example.com',
      username: 'alice',
      compromisedInfo: {
        compromiseTime: COMPROMISE_TIME,
        elapsedTimeSinceCompromise: '3 days ago',
        compromiseType: 'LEAKED',
        isMuted: false,
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
      hasStartableScript: false,
      signonRealm: 'https://example.com',
      username: 'alice',
      compromisedInfo: {
        compromiseTime: COMPROMISE_TIME,
        elapsedTimeSinceCompromise: '3 days ago',
        compromiseType: 'LEAKED',
        isMuted: false,
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
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
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
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
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
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
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
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
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
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
          },
        },
        () => {
          chrome.test.assertNoLastError();
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function muteInsecureCredentialSucceeds() {
    chrome.passwordsPrivate.muteInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
          },
        },
        () => {
          chrome.test.assertNoLastError();
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function muteInsecureCredentialFails() {
    chrome.passwordsPrivate.muteInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
          },
        },
        () => {
          chrome.test.assertLastError(
              'Could not mute the insecure credential. Probably no ' +
              'matching password could be found.');
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function unmuteInsecureCredentialSucceeds() {
    chrome.passwordsPrivate.unmuteInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: true,
          },
        },
        () => {
          chrome.test.assertNoLastError();
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function unmuteInsecureCredentialFails() {
    chrome.passwordsPrivate.unmuteInsecureCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: true,
          },
        },
        () => {
          chrome.test.assertLastError(
              'Could not unmute the insecure credential. Probably no ' +
              'matching password could be found.');
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function recordChangePasswordFlowStartedManual() {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          changePasswordUrl: 'https://example.com/.well-known/change-password',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
          },
        },
        true, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function recordChangePasswordFlowStartedAutomated() {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          hasStartableScript: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          changePasswordUrl: 'https://example.com/.well-known/change-password',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
          },
        },
        false, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function recordChangePasswordFlowStartedAppNoUrl() {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(
        {
          id: 0,
          formattedOrigin: 'App (com.example.app)',
          detailedOrigin: 'com.example.app',
          isAndroidCredential: true,
          hasStartableScript: false,
          signonRealm: '',
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseType: 'LEAKED',
            isMuted: false,
          },
        },
        true, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function refreshScriptsIfNecessary() {
    chrome.passwordsPrivate.refreshScriptsIfNecessary(() => {
      chrome.test.assertNoLastError();
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

  function startAutomatedPasswordChange() {
    chrome.passwordsPrivate.startAutomatedPasswordChange(
      {
        id: 0,
        formattedOrigin: 'example.com',
        detailedOrigin: 'https://example.com',
        isAndroidCredential: false,
        hasStartableScript: true,
        signonRealm: 'https://example.com',
        username: 'alice',
        changePasswordUrl: 'https://example.com/.well-known/change-password',
        compromisedInfo: {
          compromiseTime: COMPROMISE_TIME,
          elapsedTimeSinceCompromise: '3 days ago',
          compromiseType: 'LEAKED',
          isMuted: false,
        },
      },
      (status) => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq(true, status);
        chrome.test.succeed();
      });
  },

  function startAutomatedPasswordChangeWithEmptyUrl() {
    chrome.passwordsPrivate.startAutomatedPasswordChange(
      {
        id: 0,
        formattedOrigin: 'example.com',
        detailedOrigin: 'https://example.com',
        isAndroidCredential: false,
        hasStartableScript: true,
        signonRealm: 'https://example.com',
        username: 'alice',
        compromisedInfo: {
          compromiseTime: COMPROMISE_TIME,
          elapsedTimeSinceCompromise: '3 days ago',
          compromiseType: 'LEAKED',
          isMuted: false,
        },
      },
      (status) => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq(false, status);
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
