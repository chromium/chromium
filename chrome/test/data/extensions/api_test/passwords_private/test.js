// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.


const COMPROMISE_TIME = 158322960000;

const ERROR_MESSAGE_FOR_CHANGE_PASSWORD =
    'Could not change the password. Either the password is empty, the user ' +
    'is not authenticated or no matching password could be found for the ' +
    'id.';

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
        0, {username: 'new_user', password: 'new_pass'}, (credentialId) => {
          chrome.test.assertEq(0, credentialId);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithIncorrectIdFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        -1, {username: 'new_user', password: 'new_pass'}, (credentialId) => {
          chrome.test.assertEq(undefined, credentialId);
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithEmptyPasswordFails() {
    chrome.passwordsPrivate.changeSavedPassword(
        0, {username: 'new_user', password: ''}, (credentialId) => {
          chrome.test.assertEq(undefined, credentialId);
          chrome.test.assertLastError(ERROR_MESSAGE_FOR_CHANGE_PASSWORD);
          chrome.test.succeed();
        });
  },

  function changeSavedPasswordWithNoteSucceeds() {
    chrome.passwordsPrivate.changeSavedPassword(
        0, {username: 'new_user', password: 'new_pass', note: 'some note'},
        (credentialId) => {
          chrome.test.assertEq(0, credentialId);
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

  function requestCredentialsDetails() {
    chrome.passwordsPrivate.requestCredentialsDetails([0], passwords => {
      // Ensure that the callback is invoked without an error state and the
      // expected plaintext password.
      chrome.test.assertNoLastError();
      chrome.test.assertEq(1, passwords.length);
      chrome.test.assertEq('plaintext', passwords[0].password);
      chrome.test.succeed();
    });
  },

  function requestCredentialsDetailsFails() {
    chrome.passwordsPrivate.requestCredentialsDetails([123], passwords => {
      // Ensure that the callback is invoked with an error state and the
      // message contains the right id.
      chrome.test.assertLastError(
          'Could not obtain password entry. Either the user is not ' +
          'authenticated or no credential with matching ids could be found.');
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
        chrome.test.assertTrue(!!entry.urls.signonRealm);
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
        chrome.test.assertTrue(!!exception.urls.signonRealm);
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
      chrome.test.assertEq(
          chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
          importResults.status);
      chrome.test.assertEq(42, importResults.numberImported);
      chrome.test.assertEq('test.csv', importResults.fileName);
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

  function getInsecureCredentials() {
    chrome.passwordsPrivate.getInsecureCredentials(
        insecureCredentials => {
          chrome.test.assertEq(2, insecureCredentials.length);

          var compromisedCredential = insecureCredentials[0];
          chrome.test.assertEq('example.com', compromisedCredential.urls.shown);
          chrome.test.assertEq(
              'https://example.com', compromisedCredential.urls.link);
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
              ['LEAKED'],
              compromisedCredential.compromisedInfo.compromiseTypes);

          var weakredential = insecureCredentials[1];
          chrome.test.assertEq('example.com', weakredential.urls.shown);
          chrome.test.assertEq('https://example.com', weakredential.urls.link);
          chrome.test.assertFalse(weakredential.isAndroidCredential);
          chrome.test.assertEq(
              'https://example.com/change-password',
              weakredential.changePasswordUrl);
          chrome.test.assertEq('bob', weakredential.username);
          chrome.test.assertEq(
              ['LEAKED'],
              compromisedCredential.compromisedInfo.compromiseTypes);
          chrome.test.succeed();
        });
  },

  function muteInsecureCredentialSucceeds() {
    chrome.passwordsPrivate.muteInsecureCredential(
        {
          id: 0,
          urls: {
            shown: 'example.com',
            link: 'https://example.com',
            signonRealm: 'https://example.com',
          },
          isAndroidCredential: false,
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseTypes: ['LEAKED'],
            isMuted: false,
          },
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
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
          urls: {
            shown: 'example.com',
            link: 'https://example.com',
            signonRealm: 'https://example.com',
          },
          isAndroidCredential: false,
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseTypes: ['LEAKED'],
            isMuted: false,
          },
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
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
          urls: {
            shown: 'example.com',
            link: 'https://example.com',
            signonRealm: 'https://example.com',
          },
          isAndroidCredential: false,
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseTypes: ['LEAKED'],
            isMuted: true,
          },
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
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
          urls: {
            shown: 'example.com',
            link: 'https://example.com',
            signonRealm: 'https://example.com',
          },
          isAndroidCredential: false,
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseTypes: ['LEAKED'],
            isMuted: true,
          },
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
        },
        () => {
          chrome.test.assertLastError(
              'Could not unmute the insecure credential. Probably no ' +
              'matching password could be found.');
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function recordChangePasswordFlowStarted() {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(
        {
          id: 0,
          urls: {
            shown: 'example.com',
            link: 'https://example.com',
            signonRealm: 'https://example.com',
          },
          isAndroidCredential: false,
          username: 'alice',
          changePasswordUrl: 'https://example.com/.well-known/change-password',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseTypes: ['LEAKED'],
            isMuted: false,
          },
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
        }, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function recordChangePasswordFlowStartedAppNoUrl() {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(
        {
          id: 0,
          urls: {
            shown: 'example.com',
            link: 'https://example.com',
            signonRealm: '',
          },
          isAndroidCredential: true,
          username: 'alice',
          compromisedInfo: {
            compromiseTime: COMPROMISE_TIME,
            elapsedTimeSinceCompromise: '3 days ago',
            compromiseTypes: ['LEAKED'],
            isMuted: false,
          },
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
        }, () => {
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

  function movePasswordsToAccount() {
    chrome.passwordsPrivate.movePasswordsToAccount([42]);
    chrome.test.succeed();
  },

  function extendAuthValidity() {
    chrome.passwordsPrivate.extendAuthValidity(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function switchBiometricAuthBeforeFillingState() {
    chrome.passwordsPrivate.switchBiometricAuthBeforeFillingState();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },

  function showAddShortcutDialog() {
    chrome.passwordsPrivate.showAddShortcutDialog();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },

  function getCredentialGroups() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertEq(list.length, 1);

      const group = list[0];
      chrome.test.assertTrue(!!group);
      chrome.test.assertTrue(group.entries.length > 0);

      var idSet = new Set();
      for (var i = 0; i < group.entries.length; ++i) {
        var entry = group.entries[i];
        chrome.test.assertTrue(!!entry);
        chrome.test.assertTrue(!!entry.urls.signonRealm);
        chrome.test.assertTrue(!!entry.urls.shown);
        chrome.test.assertTrue(!!entry.urls.link);
        idSet.add(entry.id);
      }

      // Ensure that all entry ids are unique.
      chrome.test.assertEq(group.entries.length, idSet.size);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getCredentialGroups(callback);
  },

  function getCredentialsWithReusedPassword() {
    chrome.passwordsPrivate.getCredentialsWithReusedPassword(
      credentialsGroupedByPassword => {
        chrome.test.assertEq(1, credentialsGroupedByPassword.length);

        var credentialsWithReusedPassword = credentialsGroupedByPassword[0];
        chrome.test.assertEq(2, credentialsWithReusedPassword.entries.length);

        var firstCredentials = credentialsWithReusedPassword.entries[0];
        chrome.test.assertEq('example.com', firstCredentials.urls.shown);
        chrome.test.assertEq('https://example.com', firstCredentials.urls.link);
        chrome.test.assertFalse(firstCredentials.isAndroidCredential);
        chrome.test.assertEq(
            'https://example.com/change-password',
            firstCredentials.changePasswordUrl);
        chrome.test.assertEq('bob', firstCredentials.username);
        chrome.test.assertEq(
            ['REUSED'],
            firstCredentials.compromisedInfo.compromiseTypes);

        var secondCredential = credentialsWithReusedPassword.entries[1];
        chrome.test.assertEq('test.com', secondCredential.urls.shown);
        chrome.test.assertEq('https://test.com', secondCredential.urls.link);
        chrome.test.assertFalse(secondCredential.isAndroidCredential);
        chrome.test.assertEq('angela', secondCredential.username);
        chrome.test.assertEq(
            ['REUSED'],
            secondCredential.compromisedInfo.compromiseTypes);
        chrome.test.succeed();
      });
  },

  function showExportedFileInShell() {
    chrome.passwordsPrivate.showExportedFileInShell(
        '/usr/testfolder/testfilename.csv');
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
