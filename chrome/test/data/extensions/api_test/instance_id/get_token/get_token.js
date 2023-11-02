// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getTokenShouldFail() {
  chrome.test.fail("getToken should fail due to parameter validation.");
}

function getTokenWithoutParameters() {
  try {
    chrome.instanceID.getToken();
    chrome.test.fail(
        "Calling getToken without parameters should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutCallback() {
  try {
    chrome.instanceID.getToken({"authorizedEntity": "1", "scope": "GCM"});
    chrome.test.fail(
        "Calling getToken without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutAuthorizedEntity() {
  try {
    chrome.instanceID.getToken({"scope": "GCM"}, getTokenShouldFail);
    getTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithInvalidAuthorizedEntity() {
  try {
    chrome.instanceID.getToken(
        {"authorizedEntity": 1, "scope": "GCM"}, getTokenShouldFail);
    getTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutScope() {
  try {
    chrome.instanceID.getToken({"authorizedEntity": "1"}, getTokenShouldFail);
    getTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithInvalidScope() {
  try {
    chrome.instanceID.getToken(
      {"authorizedEntity": "1", "scope": 1}, getTokenShouldFail);
    getTokenShouldFail();
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithInvalidOptionValue() {
  try {
    chrome.instanceID.getToken(
      {"authorizedEntity": "1", "scope": "GCM", "options": {"foo": 1}},
      getTokenShouldFail
    );
    getTokenShouldFail()
  } catch (e) {
    chrome.test.succeed();
  };
}

function getTokenWithoutOptions() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM"},
    function(token) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }
      if (!token) {
        chrome.test.fail("Empty token returned.");
        return;
      }

      chrome.test.succeed();
    }
  );
}

function getTokenWithValidOptions() {
  chrome.instanceID.getToken(
    {"authorizedEntity": "1", "scope": "GCM", "options": {"foo": "1"}},
    function(token) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }
      if (!token) {
        chrome.test.fail("Empty token returned.");
        return;
      }

      chrome.test.succeed();
    }
  );
}

chrome.test.runTests([
  getTokenWithoutParameters,
  getTokenWithoutCallback,
  getTokenWithoutAuthorizedEntity,
  getTokenWithInvalidAuthorizedEntity,
  getTokenWithoutScope,
  getTokenWithInvalidScope,
  getTokenWithInvalidOptionValue,
  getTokenWithoutOptions,
  getTokenWithValidOptions,
]);
