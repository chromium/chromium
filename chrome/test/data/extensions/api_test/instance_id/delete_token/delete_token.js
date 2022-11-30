// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function deleteTokenShouldFail() {
  chrome.test.fail("deleteToken should fail due to parameter validation.");
}

function deleteTokenWithoutParameters() {
  try {
    chrome.instanceID.deleteToken();
    chrome.test.fail(
        "Calling deleteToken without parameters should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithoutCallback() {
  try {
    chrome.instanceID.deleteToken({"authorizedEntity": "1", "scope": "GCM"});
    chrome.test.fail(
        "Calling deleteToken without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithoutAuthorizedEntity() {
  try {
    chrome.instanceID.deleteToken({"scope": "GCM"}, deleteTokenShouldFail);
    deleteTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithEmptyAuthorizedEntity() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": "", "scope": "GCM"}, deleteTokenShouldFail);
    deleteTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithInvalidAuthorizedEntity() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": 1, "scope": "GCM"}, deleteTokenShouldFail);
    deleteTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithoutScope() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": "1"}, deleteTokenShouldFail);
    deleteTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithEmptyScope() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": "1", "scope": ""}, deleteTokenShouldFail);
    deleteTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenWithInvalidScope() {
  try {
    chrome.instanceID.deleteToken(
        {"authorizedEntity": "1", "scope": 1}, deleteTokenShouldFail);
    deleteTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteTokenBeforeGetToken() {
  chrome.instanceID.deleteToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function() {
      if (chrome.runtime.lastError) {
        chrome.test.succeed();
        return;
      }

      chrome.test.fail(
          "deleteToken should fail on deleting a non-existent token.");
    }
  );
}

function deleteTokenAfterGetToken() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function(token) {
      if (chrome.runtime.lastError || !token) {
        chrome.test.fail(
            "chrome.runtime.lastError was set or token was empty.");
        return;
      }
      chrome.instanceID.deleteToken(
        {"authorizedEntity": "1", "scope": "GCM"},
        function() {
          if (chrome.runtime.lastError) {
            chrome.test.fail("chrome.runtime.lastError: " +
                chrome.runtime.lastError.message);
            return;
          }

          chrome.test.succeed();
        }
      );
    }
  );
}

var oldToken;
function getTokenDeleteTokeAndGetToken() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function(token) {
      if (chrome.runtime.lastError || !token) {
        chrome.test.fail(
            "chrome.runtime.lastError was set or token was empty.");
        return;
      }
      oldToken = token;
      chrome.instanceID.deleteToken(
        {"authorizedEntity": "1", "scope": "GCM"},
        function() {
          if (chrome.runtime.lastError) {
            chrome.test.fail("chrome.runtime.lastError: " +
                chrome.runtime.lastError.message);
            return;
          }

          chrome.instanceID.getToken(
            {"authorizedEntity": "1", "scope": "GCM"},
            function(token) {
              if (!token || token == oldToken) {
                chrome.test.fail(
                    "Different token should be returned after deleteToken.");
                return;
              }
              chrome.test.succeed();
            }
          );
        }
      );
    }
  );
}

chrome.test.runTests([
  deleteTokenWithoutParameters,
  deleteTokenWithoutCallback,
  deleteTokenWithoutAuthorizedEntity,
  deleteTokenWithEmptyAuthorizedEntity,
  deleteTokenWithInvalidAuthorizedEntity,
  deleteTokenWithoutScope,
  deleteTokenWithEmptyScope,
  deleteTokenWithInvalidScope,
  deleteTokenBeforeGetToken,
  deleteTokenAfterGetToken,
  getTokenDeleteTokeAndGetToken,
]);
