// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const callbackPass = chrome.test.callbackPass;

function makeCompareCallback(buf) {
  return function(array) {
    assertEq(buf.byteLength, array.length);
    for (let i = 0; i < buf.length; i++) {
      assertEq(buf[i], array[i]);
    }
  };
}

function makeBuffer() {
  const bufferSize = 128;
  const ab = new ArrayBuffer(bufferSize);
  const view = new Uint8Array(ab);
  for (let i = 0; i < bufferSize; i++) {
    view[i] = i + 3;
  }
  return view;
}

const tests = [
  function sendBuffer() {
    const view = makeBuffer();
    chrome.idltest.sendArrayBuffer(
        view.buffer, callbackPass(makeCompareCallback(view.buffer)));
  },

  function sendBufferView() {
    const view = makeBuffer();
    chrome.idltest.sendArrayBufferView(
        view, callbackPass(makeCompareCallback(view.buffer)));
  },

  function sendBufferSlice() {
    const view = makeBuffer();
    const bufferSlice = view.buffer.slice(64);
    assertEq(64, bufferSlice.byteLength);
    chrome.idltest.sendArrayBuffer(
        bufferSlice, callbackPass(makeCompareCallback(bufferSlice)));
  },

  function getBuffer() {
    chrome.idltest.getArrayBuffer(callbackPass(function(buffer) {
      assertTrue(buffer.__proto__ == (new ArrayBuffer()).__proto__);
      const view = new Uint8Array(buffer);
      const expected = 'hello world';
      assertEq(view.byteLength, expected.length);
      for (let i = 0; i < view.byteLength; i++) {
        assertTrue(expected[i] == String.fromCharCode(view[i]));
      }
    }));
  },
];

chrome.test.runTests(tests);
