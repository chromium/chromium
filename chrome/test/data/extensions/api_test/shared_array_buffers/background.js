// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Ensures that an extension background context can use SharedArrayBuffers and
  // can transfer it to a worker.
  function sendSharedArrayBufferToWorker() {
    const workerUrl = chrome.runtime.getURL('worker.js');
    let worker = new Worker(workerUrl);
    worker.onmessage = e => {
      chrome.test.assertEq('PASS', e.data);
      chrome.test.succeed();
    };

    let sab = new SharedArrayBuffer(16);
    let bufView = new Uint8Array(sab);
    for (let i = 0; i < 16; i++)
      bufView[i] = (i % 2);

    worker.postMessage(sab);

    chrome.test.assertEq(16, sab.byteLength);

    // The worker will ack on receiving the SharedArrayBuffer causing the test
    // to terminate.
  },

  // Ensure an extension can construct a shared WebAssembly.Memory instance
  // (with a SharedArrayBuffer as a backing store). This is used as a proxy to
  // test that web assembly threads are enabled for the extension.
  function constructSharedWebAssemblyMemory() {
    let memory =
        new WebAssembly.Memory({initial: 10, maximum: 100, shared: true});
    chrome.test.assertEq(
        '[object SharedArrayBuffer]',
        Object.getPrototypeOf(memory.buffer).toString());
    chrome.test.succeed();
  }
]);
