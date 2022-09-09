// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// browser_tests.exe --gtest_filter=ExtensionApiTest.PreferenceOnChangeSplit

var inIncognitoContext = chrome.extension.inIncognitoContext;
var pass = chrome.test.callbackPass;
var sendMessage = chrome.test.sendMessage;
var allowCookies = chrome.privacy.websites.thirdPartyCookiesAllowed;

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
    chrome.test.fail("Unexpected event: " + JSON.stringify(value) +
                     ', incognito: ' + inIncognitoContext);
  });
}

// Fail if |event| is fired (with any values). Because listenUntil stops
// listening when |event| has fired with all the values in |expected|, it may
// not capture superfluous unexpected events.
function listenAndFailWhen(event) {
  return chrome.test.listenForever(event, function(value) {
    chrome.test.fail("Unexpected event: " + JSON.stringify(value) +
                     ', incognito: ' + inIncognitoContext);
  });
}

// Constructs messages to be sent via chrome.test.sendMessage
function constructMessage(str, caller) {
  caller = caller || arguments.callee.caller.name;
  var incognitoStr = inIncognitoContext ? " incognito " : " regular ";
  console.log(caller + incognitoStr + str);
  return caller + incognitoStr + str;
}

chrome.test.runTests([
  // Changing the regular settings when no incognito-specific settings are
  // defined should fire one event in the regular window, and two in the
  // incognito.
  function changeDefault() {
    var expected = [{
      'value': false,
      'levelOfControl': 'controlled_by_this_extension'
    }];

    if (inIncognitoContext) {
      expected.push({
        'value': false,
        'incognitoSpecific': false,
        'levelOfControl': 'controlled_by_this_extension'
      });
    }

    listenUntil(allowCookies.onChange, expected);

    sendMessage(constructMessage("ready"), pass(function() {
      if (!inIncognitoContext) {
        allowCookies.set({
          'value': false
        }, pass());
      }
    }));
  },

  // Changing incognito-specific settings should only be visible to the
  // incognito window.
  function changeIncognitoOnly() {
    if (!inIncognitoContext) {
      var done = listenAndFailWhen(allowCookies.onChange);
      sendMessage(constructMessage("listening"), done);
    } else {
      listenUntil(allowCookies.onChange, [{
        'value': true,
        'incognitoSpecific': true,
        'levelOfControl': 'controlled_by_this_extension'
      }]);
    }

    sendMessage(constructMessage("ready"), pass(function() {
      if (inIncognitoContext) {
        allowCookies.set({
          'value': true,
          'scope': 'incognito_session_only'
        }, pass(function() {
          sendMessage(constructMessage("pref set", "changeIncognitoOnly"),
                      pass())
        }));
      }
    }));
  },

  // Changing the regular settings when incognito-specific settings are
  // defined should only be visible to the regular window.
  function changeDefaultOnly() {
    if (!inIncognitoContext) {
      listenUntil(allowCookies.onChange, [{
        'value': true,
        'levelOfControl': 'controlled_by_this_extension'
      }]);
    } else {
      var done = listenAndFailWhen(allowCookies.onChange);
      sendMessage(constructMessage("listening"), done);
    }

    sendMessage(constructMessage("ready"), pass(function() {
      if (!inIncognitoContext) {
        allowCookies.set({
          'value': true
        }, pass(function() {
          sendMessage(constructMessage("pref set", "changeDefaultOnly"),
                                       pass());
        }));
      }
    }));
  },

  // Change the incognito setting back to false so that we get an event when
  // clearing the value. Should not be visible to regular window.
  function changeIncognitoOnlyBack() {
    if (!inIncognitoContext) {
      var done = listenAndFailWhen(allowCookies.onChange);
      sendMessage(constructMessage("listening"), done);
    } else {
      listenUntil(allowCookies.onChange, [{
        'value': false,
        'incognitoSpecific': true,
        'levelOfControl': 'controlled_by_this_extension'
      }]);
    }

    sendMessage(constructMessage("ready"), pass(function() {
      if (inIncognitoContext) {
        allowCookies.set({
          'value': false,
          'scope': 'incognito_session_only'
        }, pass(function() {
          sendMessage(constructMessage("pref set", "changeIncognitoOnlyBack"),
                      pass())
        }));
      }
    }));
  },

  function clearIncognito() {
    if (!inIncognitoContext) {
      var done = listenAndFailWhen(allowCookies.onChange);
      sendMessage(constructMessage("listening"), done);
    } else {
      listenUntil(allowCookies.onChange, [{
        'value': true,
        'incognitoSpecific': false,
        'levelOfControl': 'controlled_by_this_extension'
      }]);
    }

    sendMessage(constructMessage("ready"), pass(function() {
      if (inIncognitoContext) {
        allowCookies.clear({
          'scope': 'incognito_session_only'
        }, pass(function() {
          sendMessage(constructMessage("pref cleared", "clearIncognito"),
                      pass())
        }));
      }
    }));
  },

  function clearDefault() {
    var expected = [{
      'value': true,
      'levelOfControl': 'controllable_by_this_extension'
    }];

    if (inIncognitoContext) {
      expected[1] = {
        'value': false,
        'incognitoSpecific': false,
        'levelOfControl': 'controllable_by_this_extension'
      };
    }

    listenUntil(allowCookies.onChange, expected);

    sendMessage(constructMessage("ready"), pass(function() {
      if (!inIncognitoContext)
        allowCookies.clear({}, pass());
    }));
  }
]);
