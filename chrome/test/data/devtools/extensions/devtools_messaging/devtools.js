// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function output(msg) {
  chrome.devtools.inspectedWindow.eval("console.log(unescape('" +
      escape(msg) + "'));")
}

var hadErrors = false;

function assertEquals(expected, actual) {
  if (expected === actual)
    return;
  output("FAIL: expected '" + expected + "', got '" + actual + "'");
  hadErrors = true;
  throw "assertion failed";
}

function completeTest() {
  if (!hadErrors)
    output("PASS");
}

function step1() {
  chrome.extension.sendRequest("foo", function(response) {
    assertEquals('onRequest callback: "foo"', response);
    step2();
  });
}

function step2() {
  var object = { "string": "foo", "number": 42 };
  chrome.extension.sendRequest(object, function(response) {
    assertEquals('onRequest callback: ' + JSON.stringify(object), response);
    step3();
  });
}

function step3() {
  function onMessage(message) {
    assertEquals("port.onMessage: foo", message);
    completeTest();
  }
  var port = chrome.runtime.connect();
  port.onMessage.addListener(onMessage);
  port.postMessage("foo");
}

step1();
