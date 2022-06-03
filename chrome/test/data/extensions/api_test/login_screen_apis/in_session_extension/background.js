// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
