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

  function addPasswordOperationDisabledByPolicy() {
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
          chrome.test.assertLastError(
              'Operation failed because CredentialsEnableService policy is ' +
              'set to false by admin.');
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

  async function changeCredentialChangePassword() {
    let groups = await chrome.passwordsPrivate.getCredentialGroups();
    let credential = groups[0].entries[0];
    chrome.test.assertFalse(credential.isPasskey);

    credential.username = 'anya';
    credential.password = 'secret';
    credential.note = 'note';
    await chrome.passwordsPrivate.changeCredential(credential);

    groups = await chrome.passwordsPrivate.getCredentialGroups();
    credential = groups[0].entries.find(entry => entry.username == 'anya');
    chrome.test.assertTrue(!!credential);
    chrome.test.assertEq(credential.note, 'note');
    chrome.test.succeed();
  },

  async function changeCredentialChangePasskey() {
    let groups = await chrome.passwordsPrivate.getCredentialGroups();
    let credential = groups[0].entries.find(credential => credential.isPasskey);

    credential.username = 'anya';
    credential.displayName = 'Anya Forger';
    await chrome.passwordsPrivate.changeCredential(credential);

    groups = await chrome.passwordsPrivate.getCredentialGroups();
    credential = groups[0].entries.find(entry => entry.username == 'anya');
    chrome.test.assertTrue(!!credential);
    chrome.test.assertEq(credential.displayName, 'Anya Forger');
    chrome.test.succeed();
  },

  async function changeCredentialNotFound() {
    const expected =
        'Error: Could not change the credential. Either the arguments are ' +
        'not valid or the credential does not exist';
    await chrome.test.assertPromiseRejects(
        chrome.passwordsPrivate.changeCredential({
          id: 42,
          affiliatedDomains: [{
            name: 'example.com',
            url: 'https://example.com',
            signonRealm: 'https://example.com',
          }],
          isPasskey: false,
          username: 'alice',
          storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
          note: '',
        }),
        expected);
    chrome.test.succeed();
  },

  function removeAndUndoRemoveSavedPassword() {
    var numCalls = 0;
    var numSavedPasswords;
    var callback = function(savedPasswordsList) {
      numCalls++;

      if (numCalls == 1) {
        numSavedPasswords = savedPasswordsList.length;
        chrome.passwordsPrivate.removeCredential(savedPasswordsList[0].id,
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

  function removePasskey() {
    var numCalls = 0;
    var numSavedCredentials;
    var callback = function(credentials) {
      numCalls++;

      if (numCalls == 1) {
        numSavedCredentials = credentials.length;
        var passkey = credentials[numSavedCredentials - 1];
        chrome.test.assertTrue(passkey.isPasskey);
        chrome.passwordsPrivate.removeCredential(passkey.id,
                                                 passkey.storedIn);
      } else if (numCalls == 2) {
        chrome.test.assertEq(credentials.length, numSavedCredentials - 1);
        chrome.test.assertEq(credentials.find(c => c.isPasskey), undefined);
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
        chrome.test.assertEq(1, entry.affiliatedDomains.length);
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

  function fetchFamilyMembers() {
    let callback = function(familyFetchResults) {
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(!!familyFetchResults);
      chrome.test.assertEq(
          chrome.passwordsPrivate.FamilyFetchStatus.SUCCESS,
          familyFetchResults.status);
      chrome.test.succeed();
    };
    chrome.passwordsPrivate.fetchFamilyMembers(callback);
  },

  function sharePassword() {
    chrome.passwordsPrivate.sharePassword(
        42, [{
          userId: 'user-id',
          email: 'user@example.com',
          displayName: 'New User',
          profileImageUrl: 'data://image/url',
          isEligible: true,
          publicKey: {
            value: 'test',
            version: 47,
          }
        }],
        () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
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

  function importPasswordsOperationDisabledByPolicy() {
    chrome.passwordsPrivate.importPasswords(
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE, () => {
          chrome.test.assertLastError(
              'Operation failed because CredentialsEnableService policy is ' +
              'set to false by admin.');
          chrome.test.succeed();
        });
  },

  function continueImport() {
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
    chrome.passwordsPrivate.continueImport([0, 1], callback);
  },

  function resetImporter() {
    chrome.passwordsPrivate.resetImporter(false, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function exportPasswords() {
    let callback = function() {
      chrome.test.assertNoLastError();

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.exportPasswords(callback);
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

  function accountStorageIsDisabled() {
    var callback = function(enabled) {
      chrome.test.assertEq(enabled, false);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isAccountStorageEnabled(callback);
  },

  function accountStorageIsEnabled() {
    var callback = function(enabled) {
      chrome.test.assertEq(enabled, true);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isAccountStorageEnabled(callback);
  },

  function enableAccountStorage() {
    chrome.passwordsPrivate.setAccountStorageEnabled(true);
    chrome.passwordsPrivate.isAccountStorageEnabled(function(enabled) {
      chrome.test.assertEq(enabled, true);
      chrome.test.succeed();
    });
  },

  function disableAccountStorage() {
    chrome.passwordsPrivate.setAccountStorageEnabled(false);
    chrome.passwordsPrivate.isAccountStorageEnabled(function(enabled) {
      chrome.test.assertEq(enabled, false);
      chrome.test.succeed();
    });
  },

  function getInsecureCredentials() {
    chrome.passwordsPrivate.getInsecureCredentials(
        insecureCredentials => {
          chrome.test.assertEq(2, insecureCredentials.length);

          var compromisedCredential = insecureCredentials[0];
          chrome.test.assertEq(
              1, compromisedCredential.affiliatedDomains.length);
          chrome.test.assertEq(
              'example.com', compromisedCredential.affiliatedDomains[0].name);
          chrome.test.assertEq(
              'https://example.com',
              compromisedCredential.affiliatedDomains[0].url);
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
          chrome.test.assertEq(1, weakredential.affiliatedDomains.length);
          chrome.test.assertEq(
              'example.com', weakredential.affiliatedDomains[0].name);
          chrome.test.assertEq(
              'https://example.com', weakredential.affiliatedDomains[0].url);
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
          affiliatedDomains: [{
            name: 'example.com',
            url: 'https://example.com',
            signonRealm: 'https://example.com',
          }],
          isPasskey: false,
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
          affiliatedDomains: [{
            name: 'example.com',
            url: 'https://example.com',
            signonRealm: 'https://example.com',
          }],
          isPasskey: false,
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
          affiliatedDomains: [{
            name: 'example.com',
            url: 'https://example.com',
            signonRealm: 'https://example.com',
          }],
          isPasskey: false,
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
          affiliatedDomains: [{
            name: 'example.com',
            url: 'https://example.com',
            signonRealm: 'https://example.com',
          }],
          isPasskey: false,
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
    chrome.passwordsPrivate.switchBiometricAuthBeforeFillingState(_ => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
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
        chrome.test.assertEq(1, entry.affiliatedDomains.length);
        idSet.add(entry.id);
      }

      // The last entry should be a passkey.
      var passkey = group.entries[group.entries.length - 1];
      chrome.test.assertTrue(passkey.isPasskey);
      chrome.test.assertEq(passkey.displayName, 'displayName');
      chrome.test.assertEq(passkey.creationTime, 1000);

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
        chrome.test.assertEq(1, firstCredentials.affiliatedDomains.length);
        chrome.test.assertEq(
            'example.com', firstCredentials.affiliatedDomains[0].name);
        chrome.test.assertEq(
            'https://example.com', firstCredentials.affiliatedDomains[0].url);
        chrome.test.assertEq(
            'https://example.com/change-password',
            firstCredentials.changePasswordUrl);
        chrome.test.assertEq('bob', firstCredentials.username);
        chrome.test.assertEq(
            ['REUSED'],
            firstCredentials.compromisedInfo.compromiseTypes);

        var secondCredential = credentialsWithReusedPassword.entries[1];
        chrome.test.assertEq(1, secondCredential.affiliatedDomains.length);
        chrome.test.assertEq(
            'test.com', secondCredential.affiliatedDomains[0].name);
        chrome.test.assertEq(
            'https://test.com', secondCredential.affiliatedDomains[0].url);
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

  function changePasswordManagerPin() {
    chrome.passwordsPrivate.changePasswordManagerPin(success => {
      chrome.test.assertFalse(success);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function isPasswordManagerPinAvailable() {
    var callback = function(available) {
      chrome.test.assertFalse(available);
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isPasswordManagerPinAvailable(callback);
  },

  function disconnectCloudAuthenticator() {
    chrome.passwordsPrivate.disconnectCloudAuthenticator(success => {
      chrome.test.assertFalse(success);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function deleteAllPasswordManagerData() {
    chrome.passwordsPrivate.deleteAllPasswordManagerData(success => {
      chrome.test.assertTrue(success);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function isConnectedToCloudAuthenticator() {
    var callback = function(connected) {
      chrome.test.assertFalse(connected);
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isConnectedToCloudAuthenticator(callback);
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
