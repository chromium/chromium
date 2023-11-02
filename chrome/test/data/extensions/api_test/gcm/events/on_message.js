// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function onMessage() {
      var expectedCalls = 4;
      var fromAndCollapseKeyTested = false;
      var fromTested = false;
      var collapseKeyTested = false;
      var regularMessageTested = false;
      var eventHandler = function(message) {
        var hasFrom = false;
        var hasCollapseKey = false;
        if (message.hasOwnProperty('from')) {
          // Test with from.
          chrome.test.assertEq('12345678', message.from);
          hasFrom = true;
        }
        if (message.hasOwnProperty('collapseKey')) {
          // Test with a collapse key.
          chrome.test.assertEq('collapseKeyValue', message.collapseKey);
          hasCollapseKey = true;
        }

        if (hasFrom && hasCollapseKey) {
          fromAndCollapseKeyTested = true;
        } else if (hasFrom) {
          fromTested = true;
        } else if (hasCollapseKey) {
          collapseKeyTested = true;
        } else {
          regularMessageTested = true;
        }

        // The message is expected to carry data regardless of other optional
        // fields.
        chrome.test.assertEq(2, Object.keys(message.data).length);
        chrome.test.assertTrue(message.data.hasOwnProperty('property1'));
        chrome.test.assertTrue(message.data.hasOwnProperty('property2'));
        chrome.test.assertEq('value1', message.data.property1);
        chrome.test.assertEq('value2', message.data.property2);

        --expectedCalls;
        if (expectedCalls == 0) {
          chrome.gcm.onMessage.removeListener(eventHandler);
          if (fromAndCollapseKeyTested && fromTested && collapseKeyTested &&
              regularMessageTested) {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        }
      };
      chrome.gcm.onMessage.addListener(eventHandler);
    }
  ]);
};
