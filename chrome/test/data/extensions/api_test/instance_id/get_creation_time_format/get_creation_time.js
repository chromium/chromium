// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Regression test for crbug.com/400486346.
  async function getCreationTimeInMilliseconds() {
    // Record time just before the call.
    const timeBeforeGetCreationTimeCall = Date.now();
    // getID() must be called first in order to generate an instanceID with a
    // valid epoch value. Otherwise getCreationTime() === 0.
    chrome.instanceID.getID(function(id) {
      chrome.instanceID.getCreationTime(function(creationTime) {
        // Record time just after the call.
        const timeAfterGetCreationTimeCall = Date.now();

        // If the time was in a different unit (like seconds) this should fail.
        // `creationTime` may have sub-millisecond precision, while Date.now()
        // returns whole milliseconds that gin::TimeClamper may round up, so
        // compare at millisecond granularity. The lower bound allows 1ms of
        // slack because on Android `creationTime` is a floored integer
        // millisecond, so a rounded-up `timeBeforeGetCreationTimeCall` can
        // exceed it by 1.
        if (typeof creationTime === 'number' &&
            creationTime >= timeBeforeGetCreationTimeCall - 1 &&
            Math.floor(creationTime) <= timeAfterGetCreationTimeCall) {
          chrome.test.succeed();
        } else {
          chrome.test.fail(
              `Creation time should be a number in milliseconds since the epoch,
             and not in the future. Got: ${creationTime}`);
        }
      });
    });
  },

]);
