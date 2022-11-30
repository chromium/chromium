// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(data) {
  if (data.actionData.isLockScreenAction)
    return;

  chrome.test.runTests([
    function launchTest() {
      chrome.test.assertTrue(!!data);
      chrome.test.assertTrue(!!data.actionData);
      chrome.test.assertEq('new_note', data.actionData.actionType);

      chrome.app.window.create('app.html', {
        lockScreenAction: 'new_note'
      }, chrome.test.callbackFail(
            'The lockScreenAction option requires lock screen app context.'));
    },
  ]);
});
