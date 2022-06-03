// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const cannotCreateMultipleWindowsErrorMessage =
    'Login screen extension UI already in use.';
const cannotCloseNoWindowErrorMessage = 'No open window to close.';
const cannotAccessLocalStorageErrorMessage =
    '"local" is not available for login screen extensions';
const cannotAccessSyncStorageErrorMessage =
    '"sync" is not available for login screen extensions';
const noManagedGuestSessionAccountsErrorMessage =
    'No managed guest session accounts';
const alreadyExistsActiveSessionErrorMessage =
    'There is already an active session';
const sessionNotLockedErrorMessage = 'Session is not locked';
const sessionNotActiveErrorMessage = 'Session is not active';
const noPermissionToUnlockErrorMessage =
    'The extension does not have permission to unlock this session';
const authenticationFailedErrorMessage = 'Authentication failed';
const i18nMessageName = 'message';

const tests = {
  /* LoginScreenUi ************************************************************/
  'LoginScreenUiCanOpenWindow': () => {
    chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  'LoginScreenUiCannotOpenMultipleWindows': () => {
    chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
        chrome.test.assertLastError(cannotCreateMultipleWindowsErrorMessage);
        chrome.test.succeed();
      });
    });
  },
  'LoginScreenUiCanOpenAndCloseWindow': () => {
    chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.loginScreenUi.close(() => {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
    });
  },
  'LoginScreenUiCannotCloseNoWindow': () => {
    chrome.loginScreenUi.close(() => {
      chrome.test.assertLastError(cannotCloseNoWindowErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginScreenUiUserCanCloseWindow': () => {
    chrome.loginScreenUi.show({url: 'some/path.html',
        userCanClose: true}, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  'LoginScreenUiUserCannotCloseWindow': () => {
    chrome.loginScreenUi.show({url: 'some/path.html',
        userCanClose: false}, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  /* Storage ******************************************************************/
  'StorageCannotAccessLocalStorage': () => {
    chrome.storage.local.get(() => {
      chrome.test.assertLastError(cannotAccessLocalStorageErrorMessage);
      chrome.storage.local.set({foo: 'bar'}, () => {
        chrome.test.assertLastError(cannotAccessLocalStorageErrorMessage);
        chrome.test.succeed();
      });
    });
  },
  'StorageCannotAccessSyncStorage': () => {
    chrome.storage.sync.get(() => {
      chrome.test.assertLastError(cannotAccessSyncStorageErrorMessage);
      chrome.storage.sync.set({foo: 'bar'}, () => {
        chrome.test.assertLastError(cannotAccessSyncStorageErrorMessage);
        chrome.test.succeed();
      });
    });
  },
  'StorageCanAccessManagedStorage': () => {
    chrome.storage.managed.get(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  /* Login ********************************************************************/
  'LoginLaunchManagedGuestSession': () => {
    chrome.login.launchManagedGuestSession(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  'LoginLaunchManagedGuestSessionWithPassword': () => {
    chrome.test.getConfig(config => {
      chrome.login.launchManagedGuestSession(config.customArg, () => {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
    });
  },
  'LoginLaunchManagedGuestSessionAlreadyExistsActiveSession': () => {
    chrome.login.launchManagedGuestSession(() => {
      chrome.test.assertLastError(alreadyExistsActiveSessionErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginLaunchManagedGuestSessionNoAccounts': () => {
    chrome.login.launchManagedGuestSession(() => {
      chrome.test.assertLastError(noManagedGuestSessionAccountsErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginExitCurrentSession': () => {
    chrome.test.getConfig(config => {
      chrome.login.exitCurrentSession(config.customArg);
      // No check for success as browser process exists.
    });
  },
  'LoginFetchDataForNextLoginAttempt': () => {
    chrome.test.getConfig(config => {
      chrome.login.fetchDataForNextLoginAttempt(data => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq(config.customArg, data);
        chrome.test.succeed();
      });
    });
  },
  'LoginLockManagedGuestSessionNotActive': () => {
    chrome.login.lockManagedGuestSession(() => {
      chrome.test.assertLastError(sessionNotActiveErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginUnlockManagedGuestSession': () => {
    chrome.test.getConfig(config => {
      chrome.login.unlockManagedGuestSession(config.customArg, () => {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
    });
  },
  'LoginUnlockManagedGuestSessionWrongPassword': () => {
    chrome.test.getConfig(config => {
      chrome.login.unlockManagedGuestSession(config.customArg, () => {
        chrome.test.assertLastError(authenticationFailedErrorMessage);
        chrome.test.succeed();
      });
    });
  },
  'LoginUnlockManagedGuestSessionNotLocked': () => {
    chrome.login.unlockManagedGuestSession('dummy_password', () => {
      chrome.test.assertLastError(sessionNotLockedErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginUnlockManagedGuestSessionNotManagedGuestSession': () => {
    chrome.login.unlockManagedGuestSession('dummy_password', () => {
      chrome.test.assertLastError(noPermissionToUnlockErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginUnlockManagedGuestSessionWrongExtensionId': () => {
    chrome.login.unlockManagedGuestSession('dummy_password', () => {
      chrome.test.assertLastError(noPermissionToUnlockErrorMessage);
      chrome.test.succeed();
    });
  },
  /* I18n *********************************************************************/
  'I18nGetMessage': () => {
    chrome.test.getConfig(config => {
      const message = chrome.i18n.getMessage(i18nMessageName);
      chrome.test.assertEq(config.customArg, message);
      chrome.test.succeed();
    });
  },
};

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
