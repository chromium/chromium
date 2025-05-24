// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests
// --gtest_filter=ExtensionPreferenceApiTest.ThirdPartyCookiesAllowedIncognito

var pw = chrome.privacy.websites;

const thirdPartyCookiesAllowedIncognitoErrorMessage =
    'Third-party cookies are blocked in incognito and cannot be re-allowed.'

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function thirdPartyCookiesAllowedTrueIncognito() {
    pw.thirdPartyCookiesAllowed.set(
        {'value': true, 'scope': 'incognito_persistent'},
        chrome.test.callbackFail(
            thirdPartyCookiesAllowedIncognitoErrorMessage, () => {
              pw.thirdPartyCookiesAllowed.get(
                  {'incognito': true},
                  expect(
                      {
                        value: false,
                        incognitoSpecific: false,
                        levelOfControl: 'controllable_by_this_extension'
                      },
                      'third-party cookies should be blocked in incognito'));
            }));
  },
  function thirdPartyCookiesAllowedFalseIncognito() {
    pw.thirdPartyCookiesAllowed.set(
        {'value': false, 'scope': 'incognito_persistent'}, function() {
          pw.thirdPartyCookiesAllowed.get(
              {'incognito': true},
              expect(
                  {
                    value: false,
                    incognitoSpecific: true,
                    levelOfControl: 'controlled_by_this_extension'
                  },
                  'third-party cookies should be blocked in incognito'));
        });
  }
]);
