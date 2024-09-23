// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The extension ID for the .../activity_log_private/friend extension, which
// this extension communicates with.  This should correspond to the public key
// defined in .../activity_log_private/friend/manifest.json.
var FRIEND_EXTENSION_ID = 'pknkgggnfecklokoggaggchhaebkajji';

// Setup the test cases.
var testCases = [];
testCases.push({
  func: function triggerApiCall() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'api_call', function response() { });
  },
  expected_activity: ['cookies.set']
});
testCases.push({
  func: function triggerSpecialCall() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'special_call', function response() { });
  },
  expected_activity: [
    'runtime.getURL',
    'extension.getViews'
  ]
});
testCases.push({
  func: function triggerDouble() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'double', function response() {});
  },
  expected_activity: ['omnibox.setDefaultSuggestion']
});
testCases.push({
  func: function triggerAppBindings() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'app_bindings', function response() { });
  },
  expected_activity: [
    // These API calls show up differently depending on whether native bindings
    // are enabled.
    /app\.[gG]etDetails/,
    /app\.[gG]etIsInstalled/,
    /app\.(getI|i)nstallState/,
  ]
});
testCases.push({
  func: function triggerObjectMethods() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'object_methods', function response() { });
  },
  expected_activity: ['storage.clear']
});
testCases.push({
  func: function triggerMessageSelf() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'message_self', function response() { });
  },
  expected_activity: [
    'runtime.sendMessage'
  ]
});
testCases.push({
  func: function triggerMessageOther() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'message_other', function response() { });
  },
  expected_activity: [
    'runtime.sendMessage'
  ]
});
testCases.push({
  func: function triggerConnectOther() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'connect_other', function response() { });
  },
  expected_activity: ['runtime.connect']
});
testCases.push({
  func: function triggerBackgroundXHR() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'background_xhr', function response() { });
  },
  expected_activity: [
    'blinkRequestResource XMLHttpRequest',
  ]
});
testCases.push({
  func: function triggerTabIds() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'tab_ids', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.move',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerTabIdsIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'tab_ids_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.move',
    'tabs.remove'
  ]
});

testCases.push({
  func: function triggerWebRequest() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'webrequest', function response() { });
  },
  expected_activity: [
    'webRequestInternal.addEventListener',
    'webRequestInternal.addEventListener',
    'webRequest.onBeforeSendHeaders/g1',
    'webRequestInternal.eventHandled',
    'webRequest.onBeforeSendHeaders',
    'webRequest.onHeadersReceived/g2',
    'webRequestInternal.eventHandled',
    'webRequest.onHeadersReceived',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ],
});

testCases.push({
  func: function triggerWebRequestIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'webrequest_incognito', function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'webRequestInternal.addEventListener',
    'webRequestInternal.addEventListener',
    'windows.create',
    'webRequest.onBeforeSendHeaders/g3',
    'webRequestInternal.eventHandled',
    'webRequest.onBeforeSendHeaders',
    'webRequest.onHeadersReceived/g4',
    'webRequestInternal.eventHandled',
    'webRequest.onHeadersReceived',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.remove'
  ],
});

testCases.push({
  func: function triggerApiCallsOnTabsUpdated() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'api_tab_updated', function response() { });
  },
  expected_activity: [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.sendMessage',
    'tabs.executeScript',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerApiCallsOnTabsUpdatedIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'api_tab_updated_incognito',
                               function response() { });
  },
  is_incognito: true,
  expected_activity: [
    'windows.create',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.sendMessage',
    'tabs.executeScript',
    'tabs.executeScript',
    'tabs.remove'
  ]
});
testCases.push({
  func: function triggerFullscreen() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'launch_dom_fullscreen',
                               function response() { });
  },
  expected_activity: [
    'runtime.getURL',
    'test.getConfig',
    'Element.webkitRequestFullscreen'
  ]
});

var domExpectedActivity = [
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.onUpdated',
    'tabs.executeScript',
     // Location access
    'blinkSetAttribute LocalDOMWindow url',
    'blinkRequestResource Main resource',
    'blinkSetAttribute LocalDOMWindow url',
    'blinkRequestResource Main resource',
    'blinkSetAttribute LocalDOMWindow url',
    'blinkRequestResource Main resource',
    'blinkSetAttribute LocalDOMWindow url',
    'blinkRequestResource Main resource',
    // Dom mutations
    // Navigator access
    'Window.navigator',
    'Geolocation.getCurrentPosition',
    'Geolocation.watchPosition',
    // Web store access - session storage
    'Window.sessionStorage',
    'Storage.setItem',
    'Storage.getItem',
    'Storage.removeItem',
    'Storage.clear',
    // Web store access - local storage
    'Window.localStorage',
    'Storage.setItem',
    'Storage.getItem',
    'Storage.removeItem',
    'Storage.clear',
    // Canvas access
    // XHR from content script.
    'blinkRequestResource XMLHttpRequest',
];

// add the hook activity
hookNames = ['click', 'dblclick', 'drag', 'dragend', 'dragenter',
             'dragleave', 'dragover', 'dragstart', 'drop', 'input',
             'keydown', 'keypress', 'keyup', 'mousedown',
             'mouseenter', 'mouseleave', 'mousemove', 'mouseout',
             'mouseover', 'mouseup', 'mousewheel'];

for (var i = 0; i < hookNames.length; i++) {
  domExpectedActivity.push('blinkAddEventListener BODY ' + hookNames[i]);
  domExpectedActivity.push('blinkAddEventListener #document ' + hookNames[i]);
  domExpectedActivity.push('blinkAddEventListener DOMWindow ' + hookNames[i]);
}

// Close the tab.
domExpectedActivity.push('tabs.remove');

testCases.push({
  func: function triggerDOMChangesOnTabsUpdated() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'dom_tab_updated', function response() { });
  },
  expected_activity: domExpectedActivity
});

testCases.push({
  func: function triggerDOMChangesOnTabsUpdatedIncognito() {
    chrome.runtime.sendMessage(FRIEND_EXTENSION_ID,
                               'dom_tab_updated_incognito',
                               function response() { });
  },
  is_incognito: true,
  expected_activity: ['windows.create'].concat(domExpectedActivity)
});

testCases.push({
  func: function checkSavedHistory() {
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = 'tabs.onUpdated';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.assertEq('tabs.onUpdated',
              result['activities'][0]['apiCall']);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function checkHistoryForURL() {
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.pageUrl = 'http://www.google.com';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function checkOtherObject() {
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'dom_access';
    filter.apiCall = 'blinkSetAttribute';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.assertEq('blinkSetAttribute',
              result['activities'][0]['apiCall']);
          chrome.test.assertEq('method',
              result['activities'][0]['other']['domVerb']);
          chrome.test.succeed();
        });
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = 'webRequest.onHeadersReceived';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(FRIEND_EXTENSION_ID,
              result['activities'][0]['extensionId']);
          chrome.test.assertEq('webRequest.onHeadersReceived',
              result['activities'][0]['apiCall']);
          chrome.test.assertEq('{"added_request_headers":true}',
              result['activities'][0]['other']['webRequest']);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function deleteActivities() {
    var activityIds = [];
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = 'tabs.executeScript';
    chrome.activityLogPrivate.getExtensionActivities(filter, function(result) {
      chrome.test.assertEq(6, result['activities'].length);
      for (var i = 0; i < result['activities'].length; i++)
        activityIds.push(result['activities'][i]['activityId']);
      chrome.test.assertEq(6, activityIds.length);
      chrome.activityLogPrivate.deleteActivities(['-1', '-2', '-3']);
      chrome.activityLogPrivate.getExtensionActivities(filter,
          function(result) {
            chrome.test.assertEq(6, result['activities'].length);
            chrome.activityLogPrivate.deleteActivities([activityIds[0]]);
            chrome.activityLogPrivate.getExtensionActivities(filter,
                function(result) {
                  chrome.test.assertEq(5, result['activities'].length);
                  for (var i = 0; i < result['activities'].length; i++)
                    chrome.test.assertFalse(activityIds[0] ==
                        result['activities'][i]['activityId']);
                  chrome.activityLogPrivate.deleteActivities(activityIds);
                  chrome.activityLogPrivate.getExtensionActivities(filter,
                      function(result) {
                        chrome.test.assertEq(0, result['activities'].length);
                        chrome.test.succeed();
                      });
                });
          });
    });
  }
});

testCases.push({
  func: function deleteGoogleUrls() {
    chrome.test.getConfig(function(config) {
      chrome.activityLogPrivate.deleteUrls(
          ['http://www.google.com:' + config.testServer.port]);

      var filter = new Object();
      filter.extensionId = FRIEND_EXTENSION_ID;
      filter.activityType = 'any';
      filter.pageUrl = 'http://www.google.com';
      chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(0, result['activities'].length);
          chrome.test.succeed();
        });
     });
  }
});

testCases.push({
  func: function deleteAllUrls() {
    chrome.activityLogPrivate.deleteUrls([]);
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.pageUrl = 'http://';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(0, result['activities'].length);
          chrome.test.succeed();
        });
  }
});

testCases.push({
  func: function deleteAllHistory() {
    chrome.activityLogPrivate.deleteDatabase();
    var filter = new Object();
    filter.extensionId = FRIEND_EXTENSION_ID;
    filter.activityType = 'any';
    filter.apiCall = '';
    chrome.activityLogPrivate.getExtensionActivities(
        filter,
        function(result) {
          chrome.test.assertEq(0, result['activities'].length);
          chrome.test.succeed();
        });
  }
});

function checkIncognito(url, incognitoExpected) {
  if (url) {
    incognitoExpected = Boolean(incognitoExpected);
    var kIncognitoMarker = '<incognito>';
    var isIncognito =
        (url.substr(0, kIncognitoMarker.length) == kIncognitoMarker);
    chrome.test.assertEq(incognitoExpected, isIncognito,
                         'Bad incognito state for URL ' + url);
  }
}

// Listener to check the expected logging is done in the test cases.
var testCaseIndx = 0;
var callIndx = -1;
var blinkArgs = {
  'blinkRequestResource': 2,
  'blinkSetAttribute': 3
};

chrome.activityLogPrivate.onExtensionActivity.addListener(
    function(activity) {
      var activityId = activity['extensionId'];
      chrome.test.assertEq(FRIEND_EXTENSION_ID, activityId);

      // Check the api call is the one we expected next.
      var apiCall = activity['apiCall'];
      if (apiCall.indexOf('blink') == 0) {
        var args = JSON.parse(activity['args']);
        if (blinkArgs[apiCall])
          args = args.splice(0, blinkArgs[apiCall] - 1);
        apiCall += ' ' + args.join(' ');
      }
      expectedCall = 'runtime.onMessageExternal';
      var testCase = testCases[testCaseIndx];
      if (callIndx > -1) {
        expectedCall = testCase.expected_activity[callIndx];
      }
      // Allow either a RegExp or a strict string comparison.
      if (expectedCall instanceof RegExp)
        chrome.test.assertTrue(expectedCall.test(apiCall));
      else
        chrome.test.assertEq(expectedCall, apiCall);

      // Check that no real URLs are logged in incognito-mode tests.  Ignore
      // the initial call to windows.create opening the tab.
      if (apiCall != 'windows.create') {
        checkIncognito(activity['pageUrl'], testCase.is_incognito);
        checkIncognito(activity['argUrl'], testCase.is_incognito);
      }

      // If all the expected calls have been logged for this test case then
      // mark as suceeded and move to the next. Otherwise look for the next
      // expected api call.
      if (callIndx == testCase.expected_activity.length - 1) {
        chrome.test.succeed();
        callIndx = -1;
        testCaseIndx++;
      } else {
        callIndx++;
      }
    }
);

chrome.test.runTests(testCases.map(testCase => testCase.func));
