// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function worker() {
    const workerPort = new SharedWorker("worker.js").port;
    workerPort.onmessage = (evt) => {
      if (evt.data != 'hullo there!') {
        chrome.test.fail();
      } else {
        chrome.test.succeed();
      }
    };
    workerPort.start();
  },

  function workerWithImport() {
    const workerPort = new SharedWorker("worker-with-import.js").port;
    workerPort.onmessage = (evt) => {
      if (evt.data != 'hullo there!') {
        chrome.test.fail();
      } else {
        chrome.test.succeed();
      }
    };
    workerPort.start();
  }
]);
