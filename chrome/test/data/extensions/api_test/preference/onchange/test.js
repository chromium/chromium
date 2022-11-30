// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.PreferenceOnChange

// Listen until |event| has fired with all of the values in |expected|.
function listenUntil(event, expected) {
  var done = chrome.test.listenForever(event, function(value) {
    for (var i = 0; i < expected.length; i++) {
      if (chrome.test.checkDeepEq(expected[i], value)) {
        expected.splice(i, 1);
        if (expected.length == 0)
          done();
        return;
      }
    }
    chrome.test.fail("Unexpected event: " + JSON.stringify(value));
  });
}

var pw = chrome.privacy.websites;
chrome.test.runTests([
  function changeDefault() {
    // Changing the regular settings when no incognito-specific settings are
    // defined should fire two events.
    listenUntil(pw.thirdPartyCookiesAllowed.onChange, [{
      'value': false,
      'levelOfControl': 'controlled_by_this_extension'
    },
    {
      'value': false,
      'incognitoSpecific': false,
      'levelOfControl': 'controlled_by_this_extension'
    }]);
    pw.thirdPartyCookiesAllowed.set({
      'value':false
    }, chrome.test.callbackPass());
  },
  function changeIncognitoOnly() {
    listenUntil(pw.thirdPartyCookiesAllowed.onChange, [{
      'value': true,
      'incognitoSpecific': true,
      'levelOfControl': 'controlled_by_this_extension'
    }]);
    pw.thirdPartyCookiesAllowed.set({
      'value': true,
      'scope': 'incognito_persistent'
    }, chrome.test.callbackPass());
  },
  function changeDefaultOnly() {
    listenUntil(pw.thirdPartyCookiesAllowed.onChange, [{
      'value': true,
      'levelOfControl': 'controlled_by_this_extension'
    }]);
    pw.thirdPartyCookiesAllowed.set({
      'value': true
    }, chrome.test.callbackPass());
  },
  function changeIncognitoOnlyBack() {
    // Change the incognito setting back to false so that we get an event when
    // clearing the value.
    listenUntil(pw.thirdPartyCookiesAllowed.onChange, [{
      'value': false,
      'incognitoSpecific': true,
      'levelOfControl': 'controlled_by_this_extension'
    }]);
    pw.thirdPartyCookiesAllowed.set({
      'value': false,
      'scope': 'incognito_persistent'
    }, chrome.test.callbackPass());
  },
  function clearIncognito() {
    listenUntil(pw.thirdPartyCookiesAllowed.onChange, [{
      'value': true,
      'incognitoSpecific': false,
      'levelOfControl': 'controlled_by_this_extension'
    }]);
    pw.thirdPartyCookiesAllowed.clear({
      'scope': 'incognito_persistent'
    }, chrome.test.callbackPass());
  },
  function clearDefault() {
    listenUntil(pw.thirdPartyCookiesAllowed.onChange, [{
      'value': true,
      'levelOfControl': 'controllable_by_this_extension'
    },
    {
      'value': false,
      'incognitoSpecific': false,
      'levelOfControl': 'controllable_by_this_extension'
    }]);
    pw.thirdPartyCookiesAllowed.clear({}, chrome.test.callbackPass());
  }
]);
