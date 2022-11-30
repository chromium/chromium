// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const FRIEND_EXTENSION_ID = 'knldjmfmopnpolahpmmgbagdohdnhkik';

var assertCorrectLog = function(activityType, apiCall, result) {
  chrome.test.assertEq(1, result.activities.length);
  var activity = result.activities[0];
  chrome.test.assertEq(activityType, activity.activityType);
  chrome.test.assertEq(apiCall, activity.apiCall);
};

chrome.test.runTests([
  // Tests runtime.onConnect api event log.
  function apiEvent() {
    chrome.activityLogPrivate.getExtensionActivities(
        {
          extensionId: FRIEND_EXTENSION_ID,
          activityType: 'api_event',
          apiCall: 'runtime.onConnect',
        },
        (result) => {
          assertCorrectLog('api_event', 'runtime.onConnect', result);
          chrome.test.succeed();
        });
  },
  // Tests runtime.connect api call log.
  function apiCall() {
    chrome.activityLogPrivate.getExtensionActivities(
        {
          extensionId: FRIEND_EXTENSION_ID,
          activityType: 'api_call',
          apiCall: 'runtime.connect',
        },
        (result) => {
          assertCorrectLog('api_call', 'runtime.connect', result);
          chrome.test.succeed();
        });
  }
]);
