// Copyright 2019 The Chromium Authors
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
  'LoginScreenUiCanOpenWindow': async () => {
    await chrome.loginScreenUi.show({url: 'some/path.html'});
    chrome.test.succeed();
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
  'LoginScreenUiCanOpenAndCloseWindow': async () => {
    await chrome.loginScreenUi.show({url: 'some/path.html'});
    await chrome.loginScreenUi.close();
    chrome.test.succeed();
  },
  'LoginScreenUiCannotCloseNoWindow': () => {
    chrome.loginScreenUi.close(() => {
      chrome.test.assertLastError(cannotCloseNoWindowErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginScreenUiUserCanCloseWindow': async () => {
    await chrome.loginScreenUi.show(
        {url: 'some/path.html', userCanClose: true});
    chrome.test.succeed();
  },
  'LoginScreenUiUserCannotCloseWindow': async () => {
    await chrome.loginScreenUi.show(
        {url: 'some/path.html', userCanClose: false});
    chrome.test.succeed();
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
  'StorageCanAccessManagedStorage': async () => {
    await chrome.storage.managed.get();
    chrome.test.succeed();
  },

  /* Login ********************************************************************/
  'LoginLaunchManagedGuestSession': async () => {
    await chrome.login.launchManagedGuestSession();
    chrome.test.succeed();
  },
  'LoginLaunchManagedGuestSessionWithPassword': async () => {
    const config = await chrome.test.getConfig();
    await chrome.login.launchManagedGuestSession(config.customArg);
    chrome.test.succeed();
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
  'LoginExitCurrentSession': async () => {
    const config = await chrome.test.getConfig();
    await chrome.login.exitCurrentSession(config.customArg);
    // No check for success as browser process exits.
  },
  'LoginFetchDataForNextLoginAttempt': async () => {
    const config = await chrome.test.getConfig();
    const data = await chrome.login.fetchDataForNextLoginAttempt();
    chrome.test.assertEq(config.customArg, data);
    chrome.test.succeed();
  },
  'LoginLockManagedGuestSessionNotActive': () => {
    chrome.login.lockManagedGuestSession(() => {
      chrome.test.assertLastError(sessionNotActiveErrorMessage);
      chrome.test.succeed();
    });
  },
  'LoginUnlockManagedGuestSession': async () => {
    const config = await chrome.test.getConfig();
    await chrome.login.unlockManagedGuestSession(config.customArg);
    // No check for success as browser process exits.
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
  'LoginRequestExternalLogout': () => {
    chrome.login.requestExternalLogout();
    chrome.test.succeed();
  },
  'LoginOnExternalLogoutDone': () => {
    chrome.login.onExternalLogoutDone.addListener(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
    chrome.test.sendMessage('onExternalLogoutDoneLoginScreenMessage');
  },
  /* I18n *********************************************************************/
  'I18nGetMessage': async () => {
    const config = await chrome.test.getConfig();
    const message = chrome.i18n.getMessage(i18nMessageName);
    chrome.test.assertEq(config.customArg, message);
    chrome.test.succeed();
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
