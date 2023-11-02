// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is used to distinguish tests for shared workers.
var isSharedWorkerTest = true;

async function expectSessionEstablished(url) {
  testWorker('expectSessionEstablished', url);
}

async function expectSessionFailed(url) {
  testWorker('expectSessionFailed', url);
}

async function testWorker(testName, url) {
  const worker = new SharedWorker('webtransport_worker.js');
  worker.port.start();
  worker.port.postMessage({test: testName, url: url});
  await new Promise((resolve) => {
    worker.port.onmessage = (message) => {
      chrome.test.assertEq('PASS', message.data);
      resolve();
    };
  });
}
