// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This script provides a helper for testing request interception
 * coverage across different element types: <webview>, <controlledframe>, and
 * <iframe>. It sets up the element, intercepts its requests, and communicates
 * with the guest page to verify that requests were observed.
 */

const kResultMessageType = 'TEST_RESULT';
const kObservedRequestMessageType = 'OBSERVED_REQUEST';
const kStartTestsMessageType = 'START_TESTS';

/**
 * Gets authentication credentials for a given URL.
 * @param {string} url
 * @return {{username: string, password: (string|null)}}
 */
function getAuthCredentials(url) {
  return {
    username: 'test',
    password: new URL(url).searchParams.get('password')
  };
}

/**
 * Starts the request interception tests for a given element type.
 * @param {string} elementType The type of element to create ('webview',
 *     'controlledframe', or 'iframe').
 * @param {string} guestUrl The URL to load in the created element.
 * @return {Promise<string>} A promise that resolves with the test result
 *     from the guest.
 */
async function startTests(elementType, guestUrl) {
  if (elementType !== 'webview' && elementType !== 'controlledframe' &&
      elementType !== 'iframe') {
    throw new Error(`Unknown element type: ${elementType}`);
  }

  const element = document.createElement(elementType);

  // Wait for the guest to finish its initial load before signaling
  // observations and starting tests.
  const loadEvent = elementType === 'iframe' ? 'load' : 'loadstop';
  const guestLoadPromise = new Promise((resolve) => {
    element.addEventListener(loadEvent, resolve);
  });

  const donePromise = guestLoadPromise.then(() => {
    const channel = new MessageChannel();
    const resultPromise = new Promise((resolve) => {
      channel.port1.onmessage = (e) => {
        if (e.data.type === kResultMessageType) {
          resolve(e.data.result);
        }
      };
    });
    element.contentWindow.postMessage(
        {type: kStartTestsMessageType}, '*', [channel.port2]);
    return resultPromise;
  });

  /**
   * Signals to the guest that a request has been observed.
   * This function waits for the guest load to complete before sending the
   * signal.
   * @param {string} url The URL of the observed request.
   * @param {string} event The name of the event handler that observed it.
   */
  const signalObservation = (url, event) => {
    guestLoadPromise.then(() => {
      element.contentWindow.postMessage(
          {type: kObservedRequestMessageType, data: {url, event}}, '*');
    });
  };

  if (elementType === 'iframe') {
    chrome.webRequest.onBeforeRequest.addListener((details) => {
      signalObservation(details.url, 'onBeforeRequest');
    }, {urls: ['*://*/*', 'ws://*/*']}, ['blocking']);
    chrome.webRequest.onAuthRequired.addListener((details) => {
      signalObservation(details.url, 'onAuthRequired');
      return {authCredentials: getAuthCredentials(details.url)};
    }, {urls: ['*://*/auth-basic*']}, ['blocking']);
  } else if (elementType === 'webview') {
    element.request.onBeforeRequest.addListener((details) => {
      signalObservation(details.url, 'onBeforeRequest');
      return {};
    }, {urls: ['*://*/*', 'ws://*/*']}, ['blocking']);
    element.request.onAuthRequired.addListener((details) => {
      signalObservation(details.url, 'onAuthRequired');
      return {authCredentials: getAuthCredentials(details.url)};
    }, {urls: ['*://*/auth-basic*']}, ['blocking']);
  } else if (elementType === 'controlledframe') {
    element.request
        .createWebRequestInterceptor({
          urlPatterns: ['*://*/*', 'ws://*/*'],
          blocking: true,
        })
        .addEventListener('beforerequest', (e) => {
          signalObservation(e.request.url, 'onBeforeRequest');
          return {};
        });
    element.request
        .createWebRequestInterceptor({
          urlPatterns: ['*://*/auth-basic*'],
          blocking: true,
        })
        .addEventListener('authrequired', (e) => {
          e.setCredentials(getAuthCredentials(e.request.url));
          signalObservation(e.request.url, 'onAuthRequired');
        });
  }

  element.setAttribute('src', guestUrl);
  document.body.appendChild(element);
  return await donePromise;
}

/**
 * Entry point for running request interception tests.
 * @param {string} elementType The type of element to test.
 * @param {string} baseUrl The base URL for the guest page.
 * @param {number} wsPort The port for WebSocket tests.
 * @param {number} wtPort The port for WebTransport tests.
 * @param {Array<string>} expectedFailureList A list of test titles that are
 *     expected to fail.
 * @return {Promise<string>}
 */
async function run_tests(
    elementType, baseUrl, wsPort, wtPort, expectedFailureList) {
  const guestUrl =
      new URL('webview/request_interception_coverage_guest.html', baseUrl);
  guestUrl.searchParams.set('ws_port', wsPort);
  guestUrl.searchParams.set('wt_port', wtPort);
  guestUrl.searchParams.set('expected_failures', expectedFailureList.join(','));

  return await startTests(elementType, guestUrl.href);
}

// Ensure run_tests is available in the global scope for ExecJs/EvalJs.
window.run_tests = run_tests;
