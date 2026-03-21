// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This script runs within the guest page (a <webview>,
 * <controlledframe>, or <iframe>) and initiates several types of network
 * requests (Fetch, WebSocket, WebTransport) from various contexts (main page,
 * Dedicated Worker, Shared Worker, Service Worker). It verifies if these
 * requests were intercepted by the embedder by communicating with it via
 * postMessage.
 */

// The timeout for tests that are expected to succeed.
const kLongObservationTimeoutMs = 2000;
// The timeout for tests that are expected to fail. This reduces wait time for
// known failures.
const kShortObservationTimeoutMs = 500;

const kParams = new URLSearchParams(window.location.search);
// The port for the WebSocket test server.
const kWebSocketPort = kParams.get('ws_port');
// The port for the WebTransport test server.
const kWebTransportPort = kParams.get('wt_port');
// A list of test titles that are expected to fail in the current environment.
// For these tests, we use a shorter timeout (kShortObservationTimeoutMs)
// instead of the default long timeout to avoid unnecessary delays and
// improve test efficiency and stability.
const kExpectedFailures = kParams.get('expected_failures').split(',');

const kWorkerScript = './request_interception_coverage_guest_worker.js';
const kWebSocketBaseUrl =
    `ws://localhost:${kWebSocketPort}/echo-with-no-extension`;
const kWebTransportBaseUrl = `https://localhost:${kWebTransportPort}/echo`;

const kResultMessageType = 'TEST_RESULT';
const kObservedRequestMessageType = 'OBSERVED_REQUEST';
const kStartTestsMessageType = 'START_TESTS';

/**
 * Loads a URL into a new <iframe>.
 * @param {string} url
 */
async function loadIframe(url) {
  const iframe = document.createElement('iframe');
  iframe.src = url;
  const loadPromise = new Promise((resolve) => {
    iframe.addEventListener('load', resolve);
  });
  document.body.appendChild(iframe);
  await loadPromise;
}

/**
 * Runs a test within a Dedicated Worker.
 * @param {Object} message Data to send to the worker.
 * @param {string} workerScript
 */
async function runTestInDedicatedWorker(message, workerScript = kWorkerScript) {
  const worker = new Worker(workerScript);
  const result = await new Promise((resolve) => {
    worker.addEventListener('message', (e) => {
      resolve(e.data);
      worker.terminate();
    });
    worker.postMessage(message);
  });

  if (result !== 'OK') {
    throw new Error(`Dedicated Worker test failed: ${result}`);
  }
}

/**
 * Runs a test within a Shared Worker.
 * @param {Object} message Data to send to the worker.
 * @param {string} workerScript
 */
async function runTestInSharedWorker(message, workerScript = kWorkerScript) {
  const worker =
      new SharedWorker(workerScript, {name: Math.random().toString()});
  worker.port.start();
  const result = await new Promise((resolve) => {
    worker.port.addEventListener('message', (e) => {
      resolve(e.data);
      worker.port.close();
    });
    worker.port.postMessage(message);
  });

  if (result !== 'OK') {
    throw new Error(`Shared Worker test failed: ${result}`);
  }
}

/**
 * Runs a test within a Service Worker.
 * @param {Object} message Data to send to the worker.
 * @param {string} workerScript
 */
async function runTestInServiceWorker(message, workerScript = kWorkerScript) {
  const scope = `./sw-scope-${Math.random()}/`;
  const registration =
      await navigator.serviceWorker.register(workerScript, {scope});
  // Wait for the worker to be ready.
  const worker =
      registration.active || registration.waiting || registration.installing;

  const channel = new MessageChannel();
  const resultPromise = new Promise((resolve) => {
    channel.port1.onmessage = (e) => resolve(e.data);
  });

  worker.postMessage(message, [channel.port2]);
  const result = await resultPromise;
  await registration.unregister();

  if (result !== 'OK') {
    throw new Error(`Service Worker test failed: ${result}`);
  }
}

const kTestFunctions = [
  {
    title: 'subresource',
    baseUrl: '/echo',
    run: async (url) => {
      await fetch(url);
    }
  },
  {
    title: 'main resource',
    baseUrl: '/empty.html',
    run: async (url) => {
      await loadIframe(url);
    }
  },
  {
    title: 'Dedicated Worker script',
    baseUrl: kWorkerScript,
    run: async (url) => {
      await runTestInDedicatedWorker({test: 'ping'}, url);
    }
  },
  {
    title: 'Shared Worker script',
    baseUrl: kWorkerScript,
    run: async (url) => {
      await runTestInSharedWorker({test: 'ping'}, url);
    }
  },
  {
    title: 'Service Worker script',
    baseUrl: kWorkerScript,
    run: async (url) => {
      await runTestInServiceWorker({test: 'ping'}, url);
    }
  },
  {
    title: 'Fetch from Dedicated Worker',
    baseUrl: './echo',
    run: async (url) => {
      await runTestInDedicatedWorker({test: 'fetch', url});
    }
  },
  {
    title: 'Fetch from Shared Worker',
    baseUrl: './echo',
    run: async (url) => {
      await runTestInSharedWorker({test: 'fetch', url});
    }
  },
  {
    title: 'Fetch from Service Worker',
    baseUrl: './echo',
    run: async (url) => {
      await runTestInServiceWorker({test: 'fetch', url});
    }
  },
  {
    title: 'WebSocket',
    baseUrl: kWebSocketBaseUrl,
    run: async (url) => {
      const ws = new WebSocket(url);
      await new Promise((resolve) => ws.onopen = resolve);
      ws.close();
    }
  },
  {
    title: 'WebSocket in Dedicated Worker',
    baseUrl: kWebSocketBaseUrl,
    run: async (url) => {
      await runTestInDedicatedWorker({test: 'WebSocket', url});
    }
  },
  {
    title: 'WebSocket in Shared Worker',
    baseUrl: kWebSocketBaseUrl,
    run: async (url) => {
      await runTestInSharedWorker({test: 'WebSocket', url});
    }
  },
  {
    title: 'WebSocket in Service Worker',
    baseUrl: kWebSocketBaseUrl,
    run: async (url) => {
      await runTestInServiceWorker({test: 'WebSocket', url});
    }
  },
  {
    title: 'WebTransport',
    baseUrl: kWebTransportBaseUrl,
    run: async (url) => {
      const transport = new WebTransport(url);
      await transport.ready;
      transport.close();
    }
  },
  {
    title: 'WebTransport in Dedicated Worker',
    baseUrl: kWebTransportBaseUrl,
    run: async (url) => {
      await runTestInDedicatedWorker({test: 'WebTransport', url});
    }
  },
  {
    title: 'WebTransport in Shared Worker',
    baseUrl: kWebTransportBaseUrl,
    run: async (url) => {
      await runTestInSharedWorker({test: 'WebTransport', url});
    }
  },
  {
    title: 'WebTransport in Service Worker',
    baseUrl: kWebTransportBaseUrl,
    run: async (url) => {
      await runTestInServiceWorker({test: 'WebTransport', url});
    }
  },
  {
    title: 'Auth request',
    baseUrl: '/auth-basic?password=pass',
    event: 'onAuthRequired',
    run: async (url) => {
      await loadIframe(url);
    }
  },
];

const observedRequests = new Set();
const requestObservers = new Map();

/**
 * Generates a unique key for a given URL and event.
 * @param {string} url
 * @param {string} event
 * @return {string}
 */
function getObservationKey(url, event) {
  return `${event}:${url}`;
}

/**
 * Called when an observation signal is received from the embedder.
 * @param {Object} data The observation data containing url and event.
 */
function onObservationReceived(data) {
  const observedKey = getObservationKey(data.url, data.event);
  observedRequests.add(observedKey);
  const keyObservers = requestObservers.get(observedKey);
  if (keyObservers) {
    keyObservers.forEach(resolve => resolve());
    requestObservers.delete(observedKey);
  }
}

/**
 * Waits until a request to the given URL is observed by the embedder.
 * Interception is signaled by the embedder via postMessage.
 * @param {string} url The URL to watch for.
 * @param {string} event The expected event that should observe the request.
 * @param {number} timeout The timeout in milliseconds.
 */
async function waitUntilRequestObserved(url, event, timeout) {
  const key = getObservationKey(url, event);
  if (observedRequests.has(key)) {
    return;
  }

  const observers = requestObservers.get(key) || [];
  requestObservers.set(key, observers);

  await new Promise((resolve, reject) => {
    observers.push(resolve);
    setTimeout(() => reject(new Error(`not observed by ${event}`)), timeout);
  });
}

/**
 * Runs the request interception coverage tests.
 * @param {MessagePort} resultPort The port to send the result to.
 */
async function runCoverageTests(resultPort) {
  const runTest = async (test) => {
    const url = new URL(test.baseUrl, location.href);
    url.searchParams.append('rand', Math.random().toString());
    await test.run(url.href);

    const expectedEvent = test.event || 'onBeforeRequest';
    const timeout = kExpectedFailures.includes(test.title) ?
        kShortObservationTimeoutMs :
        kLongObservationTimeoutMs;
    await waitUntilRequestObserved(url.href, expectedEvent, timeout);
  };

  const results = await Promise.allSettled(kTestFunctions.map(runTest));
  const failures =
      results
          .map(
              (res, i) => res.status === 'rejected' ?
                  `${kTestFunctions[i].title}: ${res.reason.message}` :
                  null)
          .filter(f => f !== null);

  resultPort.postMessage({
    type: kResultMessageType,
    result: failures.length === 0 ? 'Done' : failures.join('\n')
  });
}

window.addEventListener('message', async (e) => {
  if (e.data.type === kStartTestsMessageType) {
    await runCoverageTests(e.ports[0]);
  } else if (e.data.type === kObservedRequestMessageType) {
    onObservationReceived(e.data.data);
  }
});
