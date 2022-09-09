// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const extensionId = 'abcdefghijklmnopabcdefghijklmnop';

const extensionId1 = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';
const extensionId2 = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb';

const credentials = 'credentials';
const data = 'persistent data';

// Fixed return value of FakeSessionManagerClient::LoginScreenStorageRetrieve
// returned in tests.
const loginScreenStorageResult = 'Test';

const noPermissionToLockErrorMessage =
    'The extension does not have permission to lock this session';
const noPermissionToUnlockErrorMessage =
    'The extension does not have permission to unlock this session';

const tests = {
  'InSessionLoginLockManagedGuestSession': () => {
    chrome.login.lockManagedGuestSession(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  'InSessionLoginLockManagedGuestSessionNoPermission': () => {
    chrome.login.lockManagedGuestSession(() => {
      chrome.test.assertLastError(noPermissionToLockErrorMessage);
      chrome.test.succeed();
    });
  },
  'InSessionLoginUnlockManagedGuestSessionNoPermission': () => {
    chrome.login.unlockManagedGuestSession('dummy_password', () => {
      chrome.test.assertLastError(noPermissionToUnlockErrorMessage);
      chrome.test.succeed();
    });
  },
  'InSessionLoginNotifyExternalLogoutDone': () => {
    chrome.login.notifyExternalLogoutDone();
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  },
  'InSessionLoginOnRequestExternalLogout': () => {
    chrome.login.onRequestExternalLogout.addListener(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
    chrome.test.sendMessage('onRequestExternalLogoutInSessionMessage');
  },
  'InSessionLoginScreenStorageStorePersistentData': () => {
    chrome.loginScreenStorage.storePersistentData(
        [extensionId1, extensionId2], data, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  'InSessionLoginScreenStorageRetrievePersistentData': () => {
    chrome.loginScreenStorage.retrievePersistentData(extensionId, data => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(loginScreenStorageResult, data);
      chrome.test.succeed();
    });
  },
  'InSessionLoginScreenStorageStoreCredentials': () => {
    chrome.loginScreenStorage.storeCredentials(
        extensionId, credentials, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  'InSessionLoginScreenStorageRetrieveCredentials': () => {
    chrome.loginScreenStorage.retrieveCredentials(credentials => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(loginScreenStorageResult, credentials);
      chrome.test.succeed();
    });
  }
}

// |waitForTestName()| waits for the browser test to reply with a test name and
// runs the specified test. The browser test logic can be found at
// chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.cc
function waitForTestName(testName) {
  if (!tests.hasOwnProperty(testName) ||
      typeof tests[testName] !== 'function') {
    chrome.test.fail('Test not found: ' + testName);
    return;
  }

  chrome.test.runTests([tests[testName]]);
}

chrome.test.sendMessage('Waiting for test name', waitForTestName);
