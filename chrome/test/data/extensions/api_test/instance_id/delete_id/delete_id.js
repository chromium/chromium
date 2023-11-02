// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function deleteIDWithoutCallback() {
  try {
    chrome.instanceID.deleteID();
    chrome.test.fail(
        "Calling deleteID without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function deleteIDWithCallback() {
  chrome.instanceID.deleteID(function() {
    if (chrome.runtime.lastError) {
      chrome.test.fail(
          "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
      return;
    }

    chrome.test.succeed();
  });
}

var oldID;
function deleteAfterGetID() {
  chrome.instanceID.getID(function(id) {
    if (chrome.runtime.lastError || !id) {
      chrome.test.fail("chrome.runtime.lastError was set or ID was empty.");
      return;
    }
    oldID = id;
    chrome.instanceID.deleteID(function(creationTime) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }
      chrome.instanceID.getID(function(id) {
        if (!id || id == oldID) {
          chrome.test.fail("Different ID should be returned after deleteID.");
          return;
        }
        chrome.test.succeed();
      });
    });
  });
}

chrome.test.runTests([
  deleteIDWithoutCallback,
  deleteIDWithCallback,
  deleteAfterGetID,
]);
