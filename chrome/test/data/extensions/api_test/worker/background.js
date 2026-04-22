// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createWorker(workerFactory, onsuccess, onerror) {
  const worker = workerFactory();
  const onmessage = message => {
    if (worker.constructor === Worker) {
      worker.terminate();
    }
    onsuccess(message.data);
  };
  if (worker.constructor === Worker) {
    worker.onmessage = onmessage;
  } else {
    worker.port.onmessage = onmessage;
  }
  worker.onerror = onerror;
}

function fetchFromWorker(workerFactory, fetchUrl, allowed, rejected) {
  const worker = workerFactory();
  const onmessage = message => {
    if (message.data) {
      allowed();
    } else {
      rejected();
    }
  };
  if (worker.constructor === Worker) {
    worker.postMessage(fetchUrl);
    worker.onmessage = onmessage;
  } else {
    worker.port.postMessage(fetchUrl);
    worker.port.onmessage = onmessage;
  }
  worker.onerror = () => chrome.test.fail();
}

function fetchFromSameOriginWorkerTest(workerFactory, fetchUrl) {
  fetchFromWorker(
      workerFactory,
      fetchUrl,
      () => {
        chrome.test.succeed();
      },
      () => {
        chrome.test.fail();
      },
  );
}

function noRedirectTest(workerFactory, expectedUrl) {
  createWorker(
      workerFactory,
      workerUrl => {
        // The expected URL will be the absolute URL of the script.
        chrome.test.assertTrue(workerUrl.endsWith(expectedUrl));
        chrome.test.succeed();
      },
      () => {
        chrome.test.fail();
      },
  );
}

chrome.test.getConfig(function(config) {
  const baseUrl = 'http://127.0.0.1:' + config.testServer.port;
  const workerUrl = 'worker.js';
  const sharedWorkerUrl = 'shared_worker.js';
  const workerForFetchUrl = 'fetch.js';
  const sameOriginFetchUrl = baseUrl + '/worker/empty.js';

  chrome.test.runTests([
    fetchFromSameOriginWorkerTest.bind(
        undefined,
        () => {
          return new Worker(workerForFetchUrl);
        },
        sameOriginFetchUrl),
    fetchFromSameOriginWorkerTest.bind(
        undefined,
        () => {
          return new SharedWorker(workerForFetchUrl);
        },
        sameOriginFetchUrl),
    noRedirectTest.bind(
        undefined,
        () => {
          return new Worker(workerUrl);
        },
        workerUrl),
    noRedirectTest.bind(
        undefined,
        () => {
          return new Worker(workerUrl, {type: 'module'});
        },
        workerUrl),
    noRedirectTest.bind(
        undefined,
        () => {
          return new SharedWorker(
              sharedWorkerUrl, {name: 'noRedirectTest-classic'});
        },
        sharedWorkerUrl),
    noRedirectTest.bind(
        undefined,
        () => {
          return new SharedWorker(
              sharedWorkerUrl, {name: 'noRedirectTest-module', type: 'module'});
        },
        sharedWorkerUrl),
  ]);
});
