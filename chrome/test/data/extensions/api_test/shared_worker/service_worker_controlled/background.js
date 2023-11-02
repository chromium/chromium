// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function setup() {
  const reg = await navigator.serviceWorker.register('service_worker.js');
  await navigator.serviceWorker.ready;
  if (navigator.serviceWorker.controller) {
    chrome.test.sendMessage('CONTROLLED');
  } else {
    chrome.test.sendMessage('READY');
  }
}

async function getMessageFromWorker(worker) {
  return new Promise(resolve => {
    worker.port.onmessage = evt => {
      resolve(evt.data);
    }
  });
}

async function getMessageFromServiceWorker() {
  return new Promise(resolve => {
    navigator.serviceWorker.onmessage = evt => {
      resolve(evt.data);
    }
  });
}


async function start() {
  await setup();
  if (!navigator.serviceWorker.controller) {
    // The browser will reload this background page so it gets controlled.
    return;
  }

  // Start the shared worker. It should send a message about the resources it
  // loaded.
  const sharedWorker = new SharedWorker('shared_worker.js');
  sharedWorker.port.start();
  const kExpectedMessage = [
    'CONNECTED',
    'SCRIPT_IMPORTED',
    'FETCHED'
  ];
  const data = await getMessageFromWorker(sharedWorker);
  if (data.length != kExpectedMessage.length) {
    throw new Error('bad message length: ' +
        `expected ${kExpectedMessage.length}, got ${data.length}`);
  }
  for (let i = 0; i < data.length; i++) {
    if (data[i] != kExpectedMessage[i]) {
      throw new Error(
          `bad message: expected ${kExpectedMessage[i]}, got ${data[i]}`);
    }
  }

  // Ask the service worker what URLs it intercepted.
  navigator.serviceWorker.controller.postMessage('tell me what urls you saw');
  const urls = await getMessageFromServiceWorker();
  const kExpectedUrls = [
    'background.html',
    'background.js',
    'shared_worker.js',
    'shared_worker_import.js',
    'data_for_fetch'
  ];
  if (urls.length != kExpectedUrls.length) {
    throw new Error(
        `bad urls: expected ${kExpectedUrls.length}, got ${urls.length}`);
  }
  for (let i = 0; i < urls.length; i++) {
    const expected = new URL(kExpectedUrls[i], self.location).toString();
    if (urls[i] != expected)
      throw new Error(`bad url: expected ${expected}, got ${urls[i]}`);
  }

  chrome.test.sendMessage('PASS');
}

start().catch(err => {
     console.error(err.name + ': ' + err.message);
     chrome.test.sendMessage('FAIL');
  });
