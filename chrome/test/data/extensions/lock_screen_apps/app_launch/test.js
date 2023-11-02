// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function noAccessToIdentity() {
    chrome.test.assertTrue(!chrome.identity);
    chrome.test.succeed();
  },

  function hasAccessToCurrentWindow() {
    chrome.test.assertTrue(!!chrome.app.window.current);
    chrome.test.assertTrue(!!chrome.app.window.current());
    chrome.test.assertTrue(chrome.app.window.current().isMaximized());
    chrome.test.succeed();
  },

  function cannotCreateSecondWindow() {
    chrome.app.window.create('test.html', {
      lockScreenAction: 'new_note'
    }, chrome.test.callbackFail('Failed to create the app window.'));
  },

  function reportReadyToClose() {
    // Notify the test runner the app window is ready to be closed - if the test
    // runner replies with 'close', close the current app window. Otherwise, the
    // test runner will close the window itself.
    // NOTE: The call to chrome.test.succeed should not be blocked on receiving
    //     the response to the message - the test runner should be notified of
    //     test run success before the app window is closed to avoid test done
    //     message being disregarded due to app window going away.
    chrome.test.sendMessage('readyToClose', function(response) {
      if (response === 'close')
        chrome.app.window.current().close();
    });

    chrome.test.succeed();
  },
]);
