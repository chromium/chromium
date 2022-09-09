// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var callbackPass = chrome.test.callbackPass;

function makeCompareCallback(buf) {
  return function(array) {
    assertEq(buf.byteLength, array.length);
    for (var i = 0; i < buf.length; i++) {
      assertEq(buf[i], array[i]);
    }
  };
}

function makeBuffer() {
  var bufferSize = 128;
  var ab = new ArrayBuffer(bufferSize);
  var view = new Uint8Array(ab);
  for (var i = 0; i < bufferSize; i++) {
    view[i] = i+3;
  }
  return view;
}

var tests = [
  function sendBuffer() {
    var view = makeBuffer();
    chrome.idltest.sendArrayBuffer(
        view.buffer, callbackPass(makeCompareCallback(view.buffer)));
  },

  function sendBufferView() {
    var view = makeBuffer();
    chrome.idltest.sendArrayBufferView(
        view, callbackPass(makeCompareCallback(view.buffer)));
  },

  function sendBufferSlice() {
    var view = makeBuffer();
    var bufferSlice = view.buffer.slice(64);
    assertEq(64, bufferSlice.byteLength);
    chrome.idltest.sendArrayBuffer(
        bufferSlice, callbackPass(makeCompareCallback(bufferSlice)));
  },

  function getBuffer() {
    chrome.idltest.getArrayBuffer(callbackPass(function(buffer) {
      assertTrue(buffer.__proto__ == (new ArrayBuffer()).__proto__);
      var view = new Uint8Array(buffer);
      var expected = "hello world";
      assertEq(view.byteLength, expected.length);
      for (var i = 0; i < view.byteLength; i++) {
        assertTrue(expected[i] == String.fromCharCode(view[i]));
      }
    }));
  }
];

chrome.test.runTests(tests);
