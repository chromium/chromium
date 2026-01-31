// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const worker = new SharedWorker('fetch-from-shared-worker-as-public-address-worker.js');

let resolve;
worker.port.onmessage = e => { resolve(e.data); };
worker.port.start();

function fetch_from_shared_worker(url) {
  let p = new Promise(r => { resolve = r; });
  worker.port.postMessage({ url });
  return p;
}
