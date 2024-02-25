// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function contentScript() {
    chrome.activityLogPrivate.getExtensionActivities(
      {
        activityType: 'content_script'
      },
      (result) => {
        chrome.test.assertEq(1, result.activities.length);
        chrome.test.succeed();
      });
  }
]);
