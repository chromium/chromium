// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(data) {
  chrome.test.runTests([
    function launchTest() {
      chrome.test.assertTrue(!!data);
      chrome.test.assertTrue(!!data.actionData);
      chrome.test.assertEq('new_note', data.actionData.actionType);
      chrome.test.assertTrue(data.actionData.isLockScreenAction);

      chrome.app.window.create('test.html', {
        lockScreenAction: 'new_note'
      }, chrome.test.callbackPass(function(createdWindow) {
        chrome.test.listenOnce(createdWindow.onClosed,
                               chrome.test.callbackPass());
      }));
    },

    function cannotCreateWindowWithoutActionLaunch() {
      chrome.app.window.create('test.html', {
        lockScreenAction: 'new_note'
      }, chrome.test.callbackFail('Failed to create the app window.'));
    },

    function noIdentityAccess() {
      // Verify the app cannot access identity API in lock screen context
      // (even though it declared permission for the API).
      chrome.test.assertTrue(!chrome.identity);
      chrome.test.succeed();
    },
  ]);
});
