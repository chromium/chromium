// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Registers an associated service worker.
async function registerWorker() {
  await navigator.serviceWorker.register('./service_worker.js');
  let registration = await navigator.serviceWorker.ready;
  let worker = registration.active;
  return worker;
}

// Tells the service worker to perform a fetch to `url`, resolving to the result
// of the fetch as a string or an encountered error.
async function doFetchInWorker(url) {
  let worker = await registerWorker();
  let channel = new MessageChannel();
  let resolveMessage;
  let messagePromise = new Promise((resolve) => { resolveMessage = resolve; });
  channel.port1.onmessage = (e) => { resolveMessage(e.data); };
  worker.postMessage({command: 'fetch', url}, [channel.port2]);
  let response = await messagePromise;
  return response;
}
