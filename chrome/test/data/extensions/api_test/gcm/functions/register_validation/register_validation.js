// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateSenderIds(size) {
  var senders = [];
  for (var i = 0; i < size; i++) {
    senders.push("Sender" + i);
  }
  return senders;
}

function toArrayDefinitionString(senderIds) {
  var idsString = "[";
  senderIds.forEach(function(element, index) {
    if (index > 0) idsString += ", ";
      idsString += "\"" + element + "\"";
  });
  idsString += "]";
  return idsString;
}

var registrationCount = 0;

function registerSuccessfully(senderIds) {
  chrome.gcm.register(senderIds, function(registrationId) {
    var expectedRegistrationId = senderIds.length + "-" + (registrationCount++);
    chrome.test.assertEq("" + expectedRegistrationId, registrationId);
    chrome.test.succeed();
  });
}

function registerInvalidParameters(senderIds) {
  try {
    chrome.gcm.register(senderIds, function(registrationId) {
      chrome.test.fail("Arguments: " + toArrayDefinitionString(senderIds));
    });
  } catch (e) {
    chrome.test.succeed();
  };
}

chrome.test.runTests([
  function successWithOneSender() {
    registerSuccessfully(generateSenderIds(1));
  },
  function successWithMultipleSenders() {
    registerSuccessfully(generateSenderIds(10));
  },
  function successWithMaxSenders() {
    registerSuccessfully(generateSenderIds(100));
  },
  function failureWithNoSenders() {
    registerInvalidParameters([]);
  },
  function failureWithEmptySenderOnly() {
    registerInvalidParameters([""]);
  },
  function failureWithEmptySender() {
    registerInvalidParameters(["good", ""]);
  },
  function failureWithTooManySenders() {
    registerInvalidParameters(generateSenderIds(101));
  }
]);
