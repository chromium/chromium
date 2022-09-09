// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fail = chrome.test.callbackFail;

var GESTURE_ERROR = "This function must be called during a user gesture";

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function testPermissionsRetainGesture() {
      chrome.test.runWithUserGesture(function() {
        chrome.permissions.request(
            {permissions: ['bookmarks']},
            function(granted) {
              chrome.test.assertTrue(granted);

              // The user gesture is retained, so we can request again.
              chrome.permissions.request(
                  {permissions: ['bookmarks']},
                  function(granted) {
                    chrome.test.assertTrue(granted);

                    // The user gesture is retained but is consumed outside,
                    // so the following request will fail.
                    chrome.permissions.request(
                        {permissions: ['bookmarks']},
                        fail(GESTURE_ERROR));
                  }
              );

              // Consume the user gesture
              window.open("", "", "");
            }
        );
      });
    }
  ]);
});
