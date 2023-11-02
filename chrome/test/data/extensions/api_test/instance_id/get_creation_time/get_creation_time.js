// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getCreationTimeWithoutCallback() {
  try {
    chrome.instanceID.getCreationTime();
    chrome.test.fail(
        "Calling getCreationTime without callback should fail.");
  } catch (e) {
    chrome.test.succeed();
  };
}

function getCreationTimeBeforeGetID() {
  chrome.instanceID.getCreationTime(function(creationTime) {
    if (chrome.runtime.lastError) {
      chrome.test.fail(
          "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
      return;
    }
    if (creationTime) {
      chrome.test.fail("Creation time should be zero.");
      return;
    }

    chrome.test.succeed();
  });
}

function getCreationTimeAfterGetID() {
  chrome.instanceID.getID(function(id) {
    if (!id) {
      chrome.test.fail("ID should not be zero.");
      return;
    }
    chrome.instanceID.getCreationTime(function(creationTime) {
      if (chrome.runtime.lastError) {
        chrome.test.fail(
            "chrome.runtime.lastError: " + chrome.runtime.lastError.message);
        return;
      }
      if (!creationTime) {
        chrome.test.fail("Creation time should not be zero.");
        return;
      }

      chrome.test.succeed();
    });
  });
}

chrome.test.runTests([
  getCreationTimeWithoutCallback,
  getCreationTimeBeforeGetID,
  getCreationTimeAfterGetID,
]);
