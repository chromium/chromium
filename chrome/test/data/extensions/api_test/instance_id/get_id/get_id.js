// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getIDWithoutCallback() {
  try {
    chrome.instanceID.getID();
    chrome.test.fail("Calling getID without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getIDWithCallback() {
  chrome.instanceID.getID(function(id) {
    if (chrome.runtime.lastError) {
      chrome.test.fail(
          "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
      return;
    }
    if (id == "") {
      chrome.test.fail("Empty ID returned.");
      return;
    }

    chrome.test.succeed();
  });
}

var oldID;
function getIDTwice() {
  chrome.instanceID.getID(function(id) {
    if (!id) {
      chrome.test.fail("ID should not be zero.");
      return;
    }
    oldID = id;

    chrome.instanceID.getID(function(id) {
        if (!id || id != oldID) {
          chrome.test.fail("Same ID should be returned.");
          return;
        }
        chrome.test.succeed();
      });
  });
}

chrome.test.runTests([
  getIDWithoutCallback,
  getIDWithCallback,
  getIDTwice,
]);
