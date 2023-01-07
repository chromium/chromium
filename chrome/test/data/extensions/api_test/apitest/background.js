// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
    // TODO(battre, mkwst): add more tests.

    function testAssertEq() {
      var chromeTestFail = chrome.test.fail;
      var messages = "";
      chrome.test.fail = function (message) { messages += "\n" + message; };

      // Check that assertEq(..., null) doesn't crash.
      chrome.test.assertEq({test: 1}, null);

      // Check that assertEq(null, ...) doesn't crash.
      chrome.test.assertEq(null, {test: 1});

      chrome.test.fail = chromeTestFail;
      chrome.test.assertEq(
        "\nAPI Test Error in testAssertEq" +
        "\nActual: null" +
        "\nExpected: {\"test\":1}" +
        "\nAPI Test Error in testAssertEq" +
        "\nActual: {\"test\":1}" +
        "\nExpected: null",
        messages);
      chrome.test.notifyPass();
    }
]);
