// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createMessage() {
  return {
    messageId: "message-id",
    destinationId: "destination-id",
    timeToLive: 86400,
    data: {
      "key1": "value1",
      "key2": "value"
    }
  };
}

function successfulSend(message) {
  chrome.gcm.send(message, function(messageId) {
    chrome.test.assertEq(message.messageId, messageId);
    chrome.test.succeed();
  });
}

function unsuccessfulSend(message) {
  try {
    chrome.gcm.send(message, function(messageId) {
      chrome.test.fail(message);
    });
  } catch(e) {
    chrome.test.succeed();
  }
}

function scenario(messageMutations, send) {
  var message = createMessage();
  messageMutations.forEach(function(mutation) {
    mutation(message);
  });
  send(message);
}

function expectSuccessWhen() {
  scenario(Array.prototype.slice.call(arguments), successfulSend);
}

function expectFailureWhen() {
  scenario(Array.prototype.slice.call(arguments), unsuccessfulSend);
}

chrome.test.runTests([
  function successWhenHappyPath() {
    expectSuccessWhen(/* no changes to message here */);
  },
  function successWhenTtlIsZero() {
    expectSuccessWhen(function(message) { message.timeToLive = 0; });
  },
  function successWhenTtlIsMissing() {
    expectSuccessWhen(function(message) { delete message.timeToLive; });
  },
  function failureWhenTtlIsNegative() {
    expectFailureWhen(function(message) { message.timeToLive = -1; });
  },
  function failureWhenTtlIsTooLarge() {
    expectFailureWhen(function(message) { message.timeToLive = 86401; });
  },
  function failureWhenMessageIdMissing() {
    expectFailureWhen(function(message) { delete message.messageId; });
  },
  function failureWhenMessageIdIsEmpty() {
    expectFailureWhen(function(message) { message.messageId = ""; });
  },
  function failureWhenDestinationIdMissing() {
    expectFailureWhen(function(message) { delete message.destinationId; });
  },
  function failureWhenDestinationIdIsEmpty() {
    expectFailureWhen(function(message) { message.destinationId = ""; });
  },
  function failureWhenDataIsMissing() {
    expectFailureWhen(function(message) { delete message.data; });
  },
  function failureWhenDataIsEmpty() {
    expectFailureWhen(function(message) { message.data = {}; });
  },
  function failureWhenDataKeyIsEmpty() {
    expectFailureWhen(function(message) { message.data[""] = "value"; });
  },
  function successWhenDataKeyHasGoogDotInIt() {
    expectSuccessWhen(function(message) {
      message.data["something.goog."] = "value";
    });
  },
  function failureWhenDataKeyIsGoogDot() {
    expectFailureWhen(function(message) { message.data["goog."] = "value"; });
  },
  function failureWhenDataKeyIsGoogDotPrefixed() {
    expectFailureWhen(function(message) {
      message.data["goog.something"] = "value";
    });
  },
  function failureWhenDataKeyIsGoogDotMixedCasedPrefixed() {
    expectFailureWhen(function(message) {
      message.data["GoOg.something"] = "value";
    });
  },
  function successWhenDataKeyHasGoogleInIt() {
    expectSuccessWhen(function(message) {
      message.data["somthing.google"] = "value";
    });
  },
  function failureWhenDataKeyIsGoogle() {
    expectFailureWhen(function(message) {
      message.data["google"] = "value";
    });
  },
  function failureWhenDataKeyIsMixedCasedGoogle() {
    expectFailureWhen(function(message) {
      message.data["GoOgLe"] = "value";
    });
  },
  function failureWhenDataKeyIsGooglePrefixed() {
    expectFailureWhen(function(message) {
      message.data["googleSomething"] = "value";
    });
  },
  function failureWhenDataKeyIsCollapeKey() {
    expectFailureWhen(function(message) {
      message.data["collapse_key"] = "value";
    });
  },
  function failureWhenMessageIsTooLarge() {
    expectFailureWhen(function(message) {
      function generateString(base, len) {
        // Generates a string of size |len| by concatenating |base| multiple
        // times and trimming to |len|.
        while (base.length < len) base += base;
        return base.substring(0, len);
      }

      var source = "abcdefghijklmnopqrstuvwxyz";
      // Creates 8 * (256 + 256) == 4096 bytes of message data which together
      // with data put in by default is more than allowed max.
      var entries = 8;
      while (entries > 0) {
        var s = generateString(source + entries, 256);
        message.data[s] = s;
        --entries;
      }
    });
  }
]);
